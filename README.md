# VLD With JSON Support

此仓库fork自[derickr/vld](https://github.com/derickr/vld.git)，在尽量不改变已有代码的前提下，添加了json输出支持。

## 原README

移步至[README.rst](./README.rst)。

## 起因

为了对PHP源码做XSS检测，需要提取vld输出信息做分析。vld的原版信息输出格式不方便做提取，故自行为其添加json输出。

## JSON输出

现有test.php内容如下

```php
<!DOCTYPE html>
<html>
<head/>
<body>
<?php

function tainte($src)
{
    $dst = $src + 0;
    return "<div id='". $dst."'>content</div>";
}

$array = array();
$array[] = 'safe' ;
$array[] = $_GET['userData'] ;
$array[] = 'safe' ;
$tainted = $array[1] ;

$tainted = tainte($tainted);

echo $tainted;

?>
<h1>Hello World!</h1>
</body>
</html>
```

执行`php -dvld.active=1 -dvld.execute=0 -dvld.dump_json=1 test.php`得到以下输出。

```json
[
{
     "class": null,
     "filename": "/home/dev/test.php",
     "function name": null,
     "number of ops": 20,
     "compiled vars": ["array", "tainted"],
     "ops": {
          "line": [1, 7, 13, 14, null, 15, null, null, null, 16, null, 17, null, 19, null, null, null, 21, 24, 27],
          "#": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19],
          "*": [null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null],
          "E": ["E", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null],
          "I": [">", null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null],
          "O": [null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, ">"],
          "op_code": [40, 0, 38, 147, 137, 80, 81, 147, 137, 147, 137, 81, 38, 61, 117, 60, 38, 40, 40, 62],
          "op": ["ECHO", "NOP", "ASSIGN", "ASSIGN_DIM", "OP_DATA", "FETCH_R", "FETCH_DIM_R", "ASSIGN_DIM", "OP_DATA", "ASSIGN_DIM", "OP_DATA", "FETCH_DIM_R", "ASSIGN", "INIT_FCALL", "SEND_VAR", "DO_FCALL", "ASSIGN", "ECHO", "ECHO", "RETURN"],
          "fetch": ["", "", "", "", "", "global", "", "", "", "", "", "", "", "", "", "", "", "", "", ""],
          "ext": [null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, 0, null, null, null, null],
          "return_type": [null, null, null, null, null, "IS_TMP_VAR", "IS_TMP_VAR", null, null, null, null, "IS_TMP_VAR", null, null, null, "IS_VAR", null, null, null, null],
          "return": [null, null, null, null, null, "~5", "~6", null, null, null, null, "~8", null, null, null, "$10", null, null, null, null],
          "op1_type": ["IS_CONST (40)", null, "IS_CV", "IS_CV", "IS_CONST (34)", "IS_CONST (33)", "IS_TMP_VAR", "IS_CV", "IS_TMP_VAR", "IS_CV", "IS_CONST (25)", "IS_CV", "IS_CV", "IS_UNUSED", "IS_CV", "IS_UNUSED", "IS_CV", "IS_CV", "IS_CONST (12)", "IS_CONST (11)"],
          "op1": ["%3C%21DOCTYPE+html%3E%0A%3Chtml%3E%0A%3Chead%2F%3E%0A%3Cbody%3E%0A", null, "!0", "!0", "safe", "_GET", "~5", "!0", "~6", "!0", "safe", "!0", "!1", null, "!1", null, "!1", "!1", "%3Ch1%3EHello+World%21%3C%2Fh1%3E%0A%3C%2Fbody%3E%0A%3C%2Fhtml%3E%0A", 1],
          "op2_type": [null, null, "IS_CONST (37)", "IS_UNUSED", "IS_UNUSED", null, "IS_CONST (32)", "IS_UNUSED", "IS_UNUSED", "IS_UNUSED", "IS_UNUSED", "IS_CONST (24)", "IS_TMP_VAR", "IS_CONST (21)", "IS_UNUSED", null, "IS_VAR", null, null, null],
          "op2": [null, null, "<array>", null, null, null, "userData", null, null, null, null, 1, "~8", "tainte", null, null, "$10", null, null, null],
          "ext_op_type": [],
          "ext_op": []
     },
     "path": [[0]],
     "branch": {
          "sline": [1],
          "eline": [27],
          "sop": [0],
          "eop": [19],
          "outs": [-2]
     }
},
{
     "class": null,
     "filename": "/home/dev/test.php",
     "function name": "tainte",
     "number of ops": 7,
     "compiled vars": ["src", "dst"],
     "ops": {
          "line": [7, 9, null, 10, null, null, 11],
          "#": [0, 1, 2, 3, 4, 5, 6],
          "*": [null, null, null, null, null, null, "*"],
          "E": ["E", null, null, null, null, null, null],
          "I": [">", null, null, null, null, null, null],
          "O": [null, null, null, null, null, ">", ">"],
          "op_code": [63, 1, 38, 8, 8, 62, 62],
          "op": ["RECV", "ADD", "ASSIGN", "CONCAT", "CONCAT", "RETURN", "RETURN"],
          "fetch": ["", "", "", "", "", "", ""],
          "ext": [null, null, null, null, null, null, null],
          "return_type": ["IS_CV", "IS_TMP_VAR", null, "IS_TMP_VAR", "IS_TMP_VAR", null, null],
          "return": ["!0", "~2", null, "~4", "~5", null, null],
          "op1_type": ["IS_UNUSED", "IS_CV", "IS_CV", "IS_CONST (9)", "IS_TMP_VAR", "IS_TMP_VAR", "IS_CONST (5)"],
          "op1": [null, "!0", "!1", "%3Cdiv+id%3D%27", "~4", "~5", null],
          "op2_type": [null, "IS_CONST (12)", "IS_TMP_VAR", "IS_CV", "IS_CONST (8)", null, null],
          "op2": [null, 0, "~2", "!1", "%27%3Econtent%3C%2Fdiv%3E", null, null],
          "ext_op_type": [],
          "ext_op": []
     },
     "path": [[0]],
     "branch": {
          "sline": [7],
          "eline": [11],
          "sop": [0],
          "eop": [6],
          "outs": []
     }
}]
```

相比原版纯文本输出，对编程调用更为友好。

## TODO

- [x] ~~补上原版输出中的branch info内容。~~
- [x] ~~目前json输出走stdout，计划添加dump至文件的选项。~~
- [x] ~~对大体积json输出做内存优化，防内存错误。~~
- [x] ~~添加对class_table和function_table的支持。~~
- [ ] 提供调用脚本demo(项目于2020/7月下旬开始了重构，原脚本作废)
- [ ] 等待新需求出现(欢迎提交issues)。
