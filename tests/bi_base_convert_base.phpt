--TEST--
bi_base_convert
--FILE--
<?php
var_dump(bi_base_convert('9223372036854775807', 10, 16));
var_dump(bi_base_convert('7fffffffffffffff', 16, 10));
?>
--EXPECT--
string(16) "7fffffffffffffff"
string(19) "9223372036854775807"