#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_messagefilter.h"
#include "zend_exceptions.h"
#include <string.h>
#include <ctype.h>

zend_class_entry *messagefilter_ce;

typedef struct _messagefilter_object{
    HashTable *banned_words;
    zend_object std;
} messagefilter_object;

static inline messagefilter_object *messagefilter_from_obj(zend_object *obj){
    return (messagefilter_object*)((char*)(obj) - XtOffsetOf(messagefilter_object, std));
}

#define Z_MESSAGEFILTER_P(zv) messagefilter_from_obj(Z_OBJ_P(zv))

/* Cyrillic-Latin mapping with phonetic similarity */
typedef struct {
    unsigned int cyrillic_code;
    char latin_char;
} HomoglyphMap;

static const HomoglyphMap homoglyph_mappings[] = {
    // Choose visual when char looks identical
    {0x0425, 'X'}, {0x0445, 'x'},  // х → x (looks like x)
    {0x041E, 'O'}, {0x043E, 'o'},  // О → o (looks like o)
    {0x0410, 'A'}, {0x0430, 'a'},  // А → a (looks like a)
    
    // Choose phonetic when sound matters more
    {0x0421, 'S'}, {0x0441, 's'},  // С → s (sounds like s, not c!)
    {0x0412, 'V'}, {0x0432, 'v'},  // В → v (sounds like v, not b!)
    {0x041D, 'N'}, {0x043D, 'n'},  // Н → n (sounds like n, not h!)
    {0x0420, 'R'}, {0x0440, 'r'},  // Р → r (sounds like r, not p!)
    
    // Other chars (phonetic)
    {0x0415, 'E'}, {0x0435, 'e'},
    {0x0418, 'I'}, {0x0438, 'i'},
    {0x0423, 'U'}, {0x0443, 'u'},
    {0x0411, 'B'}, {0x0431, 'b'},
    {0x0413, 'G'}, {0x0433, 'g'},
    {0x0414, 'D'}, {0x0434, 'd'},
    {0x0417, 'Z'}, {0x0437, 'z'},
    {0x041A, 'K'}, {0x043A, 'k'},
    {0x041B, 'L'}, {0x043B, 'l'},
    {0x041C, 'M'}, {0x043C, 'm'},
    {0x041F, 'P'}, {0x043F, 'p'},
    {0x0422, 'T'}, {0x0442, 't'},
    {0x0424, 'F'}, {0x0444, 'f'},
    {0x0406, 'I'}, {0x0456, 'i'},
    
    {0, 0}
};

/* Convert a character to its Latin homoglyph if exists */
static char get_homoglyph_mapping(unsigned int codepoint){
    char result = 0;

    for(int i = 0; homoglyph_mappings[i].cyrillic_code != 0; i++){
        if(homoglyph_mappings[i].cyrillic_code == codepoint){
            return homoglyph_mappings[i].latin_char;
        }
    }
    return 0;
}

static char* normalize_string(const char *input, size_t input_len, size_t *output_len){
    size_t max_output = input_len * 2;
    char *normalized = emalloc(max_output + 1);
    size_t out_pos = 0;
    size_t i = 0;
    
    while(i < input_len){
        unsigned char c = (unsigned char)input[i];
        
        if(c < 0x80){
            if(c != ' ' && c != '.' && c != '-' && c != '_' && c != '*'){
                normalized[out_pos++] = tolower(c);
            }
            i++;
        }else if((c & 0xE0) == 0xC0 && i + 1 < input_len){
            unsigned int codepoint = ((c & 0x1F) << 6) | (input[i+1] & 0x3F);
            char mapped = get_homoglyph_mapping(codepoint);
            if(mapped){
                normalized[out_pos++] = tolower(mapped);
            }else if(codepoint >= 0x0400 && codepoint <= 0x04FF){
                if(out_pos + 2 <= max_output){
                    normalized[out_pos++] = input[i];
                    normalized[out_pos++] = input[i+1];
                }
            }
            i += 2;
        }else if((c & 0xF0) == 0xE0 && i + 2 < input_len){
            if(out_pos + 3 <= max_output){
                normalized[out_pos++] = input[i];
                normalized[out_pos++] = input[i+1];
                normalized[out_pos++] = input[i+2];
            }
            i += 3;
        }else if((c & 0xF8) == 0xF0 && i + 3 < input_len){
            if(out_pos + 4 <= max_output){
                normalized[out_pos++] = input[i];
                normalized[out_pos++] = input[i+1];
                normalized[out_pos++] = input[i+2];
                normalized[out_pos++] = input[i+3];
            }
            i += 4;
        }else{
            i++;
        }
    }
    
    normalized[out_pos] = '\0';
    *output_len = out_pos;
    return normalized;
}

static int contains_word(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len){
    if(needle_len == 0 || haystack_len < needle_len){
        return 0;
    }
    
    for(size_t i = 0; i <= haystack_len - needle_len; i++){
        if(memcmp(haystack + i, needle, needle_len) == 0){
            return 1;
        }
    }
    return 0;
}

static zend_object_handlers messagefilter_object_handlers;

static zend_object *messagefilter_create_object(zend_class_entry *ce){
    messagefilter_object *intern;
    
    intern = zend_object_alloc(sizeof(messagefilter_object), ce);
    
    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);
    
    //Initialize banned words hashtable
    ALLOC_HASHTABLE(intern->banned_words);
    zend_hash_init(intern->banned_words, 16, NULL, ZVAL_PTR_DTOR, 0);
    
    intern->std.handlers = &messagefilter_object_handlers;
    
    return &intern->std;
}

static void messagefilter_free_object(zend_object *object){
    messagefilter_object *intern = messagefilter_from_obj(object);
    
    if(intern->banned_words){
        zend_hash_destroy(intern->banned_words);
        FREE_HASHTABLE(intern->banned_words);
    }
    
    zend_object_std_dtor(&intern->std);
}

PHP_METHOD(MessageFilter, __construct){
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(MessageFilter, banWord){
    char *word;
    size_t word_len;
    messagefilter_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(word, word_len)
    ZEND_PARSE_PARAMETERS_END();
    
    if(word_len == 0){
        zend_throw_exception(zend_ce_exception, "Word cannot be empty", 0);
        RETURN_THROWS();
    }
    
    intern = Z_MESSAGEFILTER_P(ZEND_THIS);
    
    //Normalize the word
    size_t normalized_len;
    char *normalized = normalize_string(word, word_len, &normalized_len);
    
    //Store in hashtable
    zend_string *key = zend_string_init(normalized, normalized_len, 0);
    zval val;
    ZVAL_TRUE(&val);
    
    zend_hash_update(intern->banned_words, key, &val);
    
    zend_string_release(key);
    efree(normalized);
    
    RETURN_NULL();
}

PHP_METHOD(MessageFilter, checkMessage){
    char *message;
    size_t message_len;
    messagefilter_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(message, message_len)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_MESSAGEFILTER_P(ZEND_THIS);
    
    if(message_len == 0){
        RETURN_FALSE;
    }
    
    //Normalize the message
    size_t normalized_msg_len;
    char *normalized_msg = normalize_string(message, message_len, &normalized_msg_len);
    
    //Check each banned word
    zend_string *key;
    zval *val;
    int found = 0;
    
    ZEND_HASH_FOREACH_STR_KEY_VAL(intern->banned_words, key, val){
        if(key){
            if(contains_word(normalized_msg, normalized_msg_len, ZSTR_VAL(key), ZSTR_LEN(key))){
                found = 1;
                break;
            }
        }
    }
    ZEND_HASH_FOREACH_END();
    
    efree(normalized_msg);
    RETURN_BOOL(found);
}

PHP_METHOD(MessageFilter, getBannedWords){
    messagefilter_object *intern;
    zend_string *key;
    zval *val;
    
    ZEND_PARSE_PARAMETERS_NONE();
    
    intern = Z_MESSAGEFILTER_P(ZEND_THIS);
    
    array_init(return_value);
    
    ZEND_HASH_FOREACH_STR_KEY_VAL(intern->banned_words, key, val){
        if(key){
            add_next_index_str(return_value, zend_string_copy(key));
        }
    }
    ZEND_HASH_FOREACH_END();
}

PHP_METHOD(MessageFilter, unbanWord){
    char *word;
    size_t word_len;
    messagefilter_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(word, word_len)
    ZEND_PARSE_PARAMETERS_END();
    
    if(word_len == 0){
        zend_throw_exception(zend_ce_exception, "Word cannot be empty", 0);
        RETURN_THROWS();
    }
    
    intern = Z_MESSAGEFILTER_P(ZEND_THIS);
    
    ///Normalize the word
    size_t normalized_len;
    char *normalized = normalize_string(word, word_len, &normalized_len);
    
    //Remove from hashtable
    zend_string *key = zend_string_init(normalized, normalized_len, 0);
    zend_hash_del(intern->banned_words, key);
    
    zend_string_release(key);
    efree(normalized);
    
    RETURN_NULL();
}

/* Argument info */
ZEND_BEGIN_ARG_INFO_EX(arginfo_messagefilter_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_messagefilter_banWord, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, word, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_messagefilter_checkMessage, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_messagefilter_getBannedWords, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_messagefilter_unbanWord, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, word, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* Method entries */
static const zend_function_entry messagefilter_methods[] = {
    PHP_ME(MessageFilter, __construct, arginfo_messagefilter_construct, ZEND_ACC_PUBLIC)
    PHP_ME(MessageFilter, banWord, arginfo_messagefilter_banWord, ZEND_ACC_PUBLIC)
    PHP_ME(MessageFilter, checkMessage, arginfo_messagefilter_checkMessage, ZEND_ACC_PUBLIC)
    PHP_ME(MessageFilter, getBannedWords, arginfo_messagefilter_getBannedWords, ZEND_ACC_PUBLIC)
    PHP_ME(MessageFilter, unbanWord, arginfo_messagefilter_unbanWord, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* Module initialization */
PHP_MINIT_FUNCTION(messagefilter){
    zend_class_entry ce;
    
    //Register class with namespace
    INIT_CLASS_ENTRY(ce, "marusel\\MessageFilter", messagefilter_methods);
    messagefilter_ce = zend_register_internal_class(&ce);
    messagefilter_ce->create_object = messagefilter_create_object;
    
    //Initialize object handlers
    memcpy(&messagefilter_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    messagefilter_object_handlers.offset = XtOffsetOf(messagefilter_object, std);
    messagefilter_object_handlers.free_obj = messagefilter_free_object;
    
    return SUCCESS;
}

/* Module information */
PHP_MINFO_FUNCTION(messagefilter){
    php_info_print_table_start();
    php_info_print_table_header(2, "messagefilter support", "enabled");
    php_info_print_table_row(2, "Version", "1.0.0");
    php_info_print_table_row(2, "Namespace", "marusel");
    php_info_print_table_end();
}

/* Module entry */
zend_module_entry messagefilter_module_entry = {
    STANDARD_MODULE_HEADER,
    "messagefilter",
    NULL,
    PHP_MINIT(messagefilter),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(messagefilter),
    "2.0.0",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MESSAGEFILTER
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(messagefilter)
#endif