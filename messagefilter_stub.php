<?php
/**
 * PHP Stubs for MessageFilter Extension
 * 
 * Provides autocomplete and type hints for the marusel\MessageFilter PHP extension.
 * 
 * @version 2.0.0
 */

namespace marusel{
    /**
     *
     * Object for smart banned word filtering. Each instance has independent list of banned words.
     *
     *  - Word normalization: lowercase, space/punctuation removal, Cyrillic homoglyphs to Latin
     *  - Functions: banWord, unbanWord, getBannedWords, checkMessage
     */
    class MessageFilter{
        /**
         * Create a new MessageFilter instance.
         */
        public function __construct() {}

        /**
         * Add a word to the banned words list.
         *
         * @param string $word The word to ban.
         * @throws \Exception if the word is empty.
         * @return void
         */
        public function banWord(string $word): void {}

        /**
         * Remove a word from the banned words list.
         *
         * @param string $word The word to unban.
         * @throws \Exception if the word is empty.
         * @return void
         */
        public function unbanWord(string $word): void {}

        /**
         * Checks if the given message contains any banned words.
         *
         * @param string $message
         * @return bool true if message contains a banned word, otherwise false.
         */
        public function checkMessage(string $message): bool {}

        /**
         * Returns a list of all banned words.
         *
         * @return string[] array of banned words.
         */
        public function getBannedWords(): array {}
    }
}
