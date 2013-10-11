## Introduction

This document describes the main features and principles of the configuration
language called `UCL` - universal configuration language.

## Basic structure

UCL is heavily infused by `nginx` configuration as the example of a convenient configuration
system. However, UCL is fully compatible with `JSON` format and is able to parse json files.
For example, you can write the same configuration in the following ways:

* in nginx like:

```nginx
param = value;
section {
    param = value;
    param1 = value1;
    flag = true;
    number = 10k;
    time = 0.2s;
    string = "something";
    subsection {
        host = {
            host = "hostname"; 
            port = 900;
        }
        host = {
            host = "hostname";
            port = 901;
        }
    }
}
```

* or in JSON:

```json
{
    "param": "value",
    "param1": "value1",
    "flag": true,
    "subsection": {
        "host": [
        {
            "host": "hostname",
            "port": 900
        },
        {
            "host": "hostname",
            "port": 901
        }
        ]
    }
}
```

## Improvements to the json notation.

There are various things that make ucl configuration more convenient for editing than strict json:

* Braces are not necessary to enclose a top object: it is automatically treated as an object:

```json
"key": "value"
```
is equal to:
```json
{"key": "value"}
```

* There is no requirement of quotes for strings and keys, moreover, `:` may be replaced `=` or even be skipped for objects:

```nginx
key = value;
section {
    key = value;
}
```
is equal to:
```json
{
    "key": "value",
    "section": {
        "key": "value"
    }
}
```

* No commas mess: you can safely place a comma or semicolon for the last element in an array or an object:

```json
{
    "key1": "value",
    "key2": "value",
}
```

* Non-unique keys in an object are allowed and are automatically converted to the arrays internally:

```json
{
    "key": "value1",
    "key": "value2"
}
```
is converted to:
```json
{
    "key": ["value1", "value2"]
}
```

* Numbers can have suffixes to specify standard multipliers:
    + `[kKmMgG]` - standard 10 base multipliers (so `1k` is translated to 1000)
    + `[kKmMgG]b` - 2 power multipliers (so `1kb` is translated to 1024)
    + `[s|min|d|w|y]` - time multipliers, all time values are translated to float number of seconds, for example `10min` is translated to 3600.0 and `10ms` is translated to 0.01
* Booleans can be specified as `true` or `yes` or `on` and `false` or `no` or `off`.
* It is still possible to treat numbers and booleans as strings by enclosing them in double quotes.

## General improvements

UCL supports different style of comments:

* single line: `#` or `//`
* multiline: `/* ... */`

Multiline comments may be nested:
```c
# Sample single line comment
/* 
 some comment
 /* nested comment */
 end of comment
*/
```

UCL supports external macroes both multiline and single line ones:
```nginx
.macro "sometext";
.macro {
     Some long text
     ....
};
```
There are two internal macroes provided by UCL:

* `include` - read a file `/path/to/file` or an url `http://example.com/file` and include it to the current place of
UCL configuration;
* `includes` - read a file or an url like the previous macro, but fetch and check the signature file (which is obtained
by `.sig` suffix appending).

Public key (or keys) used for the last command are specified by the concrete UCL user (by rspamd for example).

## Emitter

Each UCL object can be serialized to one of the three supported formats:

* `JSON` - canonic json notation (with spaces indented structure);
* `Compacted JSON` - compact json notation (without spaces or newlines);
* `Configuration` - nginx like notation.

## Conclusion

UCL has clear design that should be very convenient for reading and writing. At the same time it is compatible with
JSON language and therefore can be used as a simple JSON parser. Macroes logic provides an ability to extend configuration
language (for example by including some lua code) and comments allows to disable or enable the parts of a configuration
quickly.
