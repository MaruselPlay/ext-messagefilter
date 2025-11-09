This is a PHP extension that provides MessageFilter class which implements smart banned word filtering.

Initially this extension was made for a PMMP server, to quckly and smartly check messages of players for banned words, however it's not limited to just this use.

I made this extension to replace my current implementation of a message filter written in PHP, as in PHP it was too slow and I had to check messages in separete thread, creating noticeable delay in sending messages to the chat. 

### Usage

```php
<?php

use marusel\MessageFilter;

//basic usage
$filter = new MessageFilter();
$filter->banWord("sinexwix");

$messages = [
  "pizda",
  "notsinexwix",
  "Погнали на sinexwix",
  "Погнали на s.i.n.e.x.w.i.x",
  "Погнали на s i n e x w i x",
  "Погнали на s i n e x w i х", //last character is russian х
  "Погнали на s i n e x w i х", //last character is russian х + english e is replaced with russian е
  "Погнали на с i n e x w i х" //last character is russian х + english e is replaced with russian е + russian с at the start
];

foreach($messages as $index => $message){
  if($filter->checkMessage($message)){
    echo "Message $index blocked!\n";
  }
}

//all messages apart "pizda" are blocked

//get banned words
$words = $filter->getBannedWords();
print_r($words);

/* ouput
(
    [0] => sinexwix
)
*/

//unban a word
$filter->unbanWord("sinexwix");
```