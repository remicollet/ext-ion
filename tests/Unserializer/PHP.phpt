--TEST--
ion\Unserializer\PHP
--EXTENSIONS--
ion
--FILE--
TEST
<?php
$o1 = ion\unserialize("[foo,{p:1}]");
$o2 = ion\unserialize("[foo,{p:1}]", $u1 = new ion\Unserializer\PHP);
$o3 = ion\unserialize("[foo,{p:1}]", $u2 = new ion\Unserializer\PHP(new ion\Reader\Options));
if ($o1 != $o2) {
	var_dump($o1, $o2);
}
var_dump($u1);
if ($u1 != $u2) {
	var_dump($u2);
}
?>
DONE
--EXPECTF--
TEST
object(ion\Unserializer\PHP)#%d (4) {
  ["readerOptions"]=>
  object(ion\Reader\Options)#%d (13) {
    ["catalog"]=>
    NULL
    ["decimalContext"]=>
    NULL
    ["onContextChange"]=>
    NULL
    ["returnSystemValues"]=>
    bool(false)
    ["newLine"]=>
    int(10)
    ["maxContainerDepth"]=>
    int(10)
    ["maxAnnotations"]=>
    int(10)
    ["maxAnnotationBuffered"]=>
    int(512)
    ["symbolThreshold"]=>
    int(16384)
    ["userValueThreshold"]=>
    int(16384)
    ["chunkThreshold"]=>
    int(16384)
    ["allocationPageSize"]=>
    int(65536)
    ["skipCharacterValidation"]=>
    bool(false)
  }
  ["multiSequence"]=>
  bool(false)
  ["callMagicUnserialize"]=>
  bool(true)
  ["callCustomUnserialize"]=>
  NULL
}
DONE
