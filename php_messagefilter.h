#ifndef PHP_MESSAGEFILTER_H
#define PHP_MESSAGEFILTER_H

extern zend_module_entry messagefilter_module_entry;
#define phpext_messagefilter_ptr &messagefilter_module_entry

#define PHP_MESSAGEFILTER_VERSION "1.0.0"

#if defined(ZTS) && defined(COMPILE_DL_MESSAGEFILTER)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif