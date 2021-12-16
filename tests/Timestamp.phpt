--TEST--
ion\Timestamp
--EXTENSIONS--
ion
--INI--
date.timezone=CET
--FILE--
TEST
<?php
use ion\Timestamp;

try {
	var_dump(new Timestamp);
} catch (Throwable) {
	echo "caught empty\n";
}
$full = "2021-12-07T14:08:51+00:00";
var_dump($t=new Timestamp(Timestamp\Precision::Day, datetime:$full),(string)$t);
var_dump($t=new Timestamp(Timestamp\Precision::Day->value, datetime:$full),(string)$t);
var_dump($t=new Timestamp(Timestamp\Precision::Min, datetime:"2020-10-01"),(string)$t);
var_dump($t=new Timestamp(Timestamp\Precision::Day, "!Y-m", "2000-10"),(string)$t);
?>
DONE
--EXPECTF--
TEST
caught empty
object(ion\Timestamp)#%d (5) {
  ["precision"]=>
  int(7)
  ["format"]=>
  string(7) "Y-m-d\T"
  ["date"]=>
  string(26) "2021-12-07 14:08:51.000000"
  ["timezone_type"]=>
  int(1)
  ["timezone"]=>
  string(6) "+00:00"
}
string(11) "2021-12-07T"
object(ion\Timestamp)#%d (5) {
  ["precision"]=>
  int(7)
  ["format"]=>
  string(7) "Y-m-d\T"
  ["date"]=>
  string(26) "2021-12-07 14:08:51.000000"
  ["timezone_type"]=>
  int(1)
  ["timezone"]=>
  string(6) "+00:00"
}
string(11) "2021-12-07T"
object(ion\Timestamp)#%d (5) {
  ["precision"]=>
  int(23)
  ["format"]=>
  string(11) "Y-m-d\TH:iP"
  ["date"]=>
  string(26) "2020-10-01 00:00:00.000000"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(3) "CET"
}
string(22) "2020-10-01T00:00+02:00"
object(ion\Timestamp)#%d (5) {
  ["precision"]=>
  int(7)
  ["format"]=>
  string(7) "Y-m-d\T"
  ["date"]=>
  string(26) "2000-10-01 00:00:00.000000"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(3) "CET"
}
string(11) "2000-10-01T"
DONE
