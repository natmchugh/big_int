--TEST--
bi_add add two numbers print the result
--FILE--
<?php
$a = '2147483648';
$b = '2147483648';
$c = bi_add($a, $b);
echo bi_to_str($c);
?>
--EXPECT--
4294967296