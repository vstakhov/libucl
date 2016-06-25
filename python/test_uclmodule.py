#!/usr/bin/env python
import json
import unittest
import ucl
import sys

# Python 3.2+
if hasattr(unittest.TestCase, 'assertRaisesRegex'):
    pass
# Python 2.7 - 3.1
elif hasattr(unittest.TestCase, 'assertRaisesRegexp'):
    unittest.TestCase.assertRaisesRegex = unittest.TestCase.assertRaisesRegexp
# Python 2.6-
else:
    import re
    def assert_raises_regex(self, exception, regexp, callable, *args, **kwds):
        try:
            callable(*args, **kwds)
        except exception as e:
            if isinstance(regexp, basestring):
                regexp = re.compile(regexp)
            if not regexp.search(str(e)):
                raise self.failureException('"%s" does not match "%s"' %
                         (regexp.pattern, str(e)))
        else:
            if hasattr(exception,'__name__'): excName = exception.__name__
            else: excName = str(exception)
            raise AssertionError("%s not raised" % excName)
    unittest.TestCase.assertRaisesRegex = assert_raises_regex

# Python 2.6-
if not hasattr(unittest.TestCase, 'assertIn'):
    def assert_in(self, member, container, msg=None):
        if member not in container:
            standardMsg = '%s not found in %s' % (safe_repr(member),
                                                  safe_repr(container))
            self.fail(self._formatMessage(msg, standardMsg))
    unittest.TestCase.assertIn = assert_in


class TestUcl(unittest.TestCase):
    def test_no_args(self):
        self.assertRaises(TypeError, lambda: ucl.load())

    def test_multi_args(self):
        self.assertRaises(TypeError, lambda: ucl.load(0,0))

    def test_none(self):
        r = ucl.load(None)
        self.assertEqual(r, None)

    def test_int(self):
        r = ucl.load("a : 1")
        self.assertEqual(ucl.load("a : 1"), { "a" : 1 } )

    def test_braced_int(self):
        self.assertEqual(ucl.load("{a : 1}"), { "a" : 1 } )

    def test_nested_int(self):
        self.assertEqual(ucl.load("a : { b : 1 }"), { "a" : { "b" : 1 } })

    def test_str(self):
        self.assertEqual(ucl.load("a : b"), {"a" : "b"})

    def test_float(self):
        self.assertEqual(ucl.load("a : 1.1"), {"a" : 1.1})

    def test_boolean(self):
        totest = (
            "a : True;" \
            "b : False"
        )
        correct = {"a" : True, "b" : False}
        self.assertEqual(ucl.load(totest), correct)

    def test_empty_ucl(self):
        r = ucl.load("{}")
        self.assertEqual(r, {})

    def test_single_brace(self):
        self.assertEqual(ucl.load("{"), {})

    def test_single_back_brace(self):
        ucl.load("}")

    def test_single_square_forward(self):
        self.assertEqual(ucl.load("["), [])

    def test_invalid_ucl(self):
        self.assertRaisesRegex(ValueError, "unfinished key$", lambda: ucl.load('{ "var"'))

    def test_comment_ignored(self):
        self.assertEqual(ucl.load("{/*1*/}"), {})

    def test_1_in(self):
        with open("../tests/basic/1.in", "r") as in1:
            self.assertEqual(ucl.load(in1.read()), {'key1': 'value'})

    def test_every_type(self):
        totest="""{
            "key1": value;
            "key2": value2;
            "key3": "value;"
            "key4": 1.0,
            "key5": -0xdeadbeef
            "key6": 0xdeadbeef.1
            "key7": 0xreadbeef
            "key8": -1e-10,
            "key9": 1
            "key10": true
            "key11": no
            "key12": yes
        }"""
        correct = {
                'key1': 'value',
                'key2': 'value2',
                'key3': 'value;',
                'key4': 1.0,
                'key5': -3735928559,
                'key6': '0xdeadbeef.1',
                'key7': '0xreadbeef',
                'key8': -1e-10,
                'key9': 1,
                'key10': True,
                'key11': False,
                'key12': True,
                }
        self.assertEqual(ucl.load(totest), correct)

    def test_validation_useless(self):
        self.assertRaises(NotImplementedError, lambda: ucl.validate("",""))


class TestUclDump(unittest.TestCase):
    def test_no_args(self):
        self.assertRaises(TypeError, lambda: ucl.dump())

    def test_multi_args(self):
        self.assertRaises(TypeError, lambda: ucl.dump(0, 0))

    def test_none(self):
        self.assertEqual(ucl.dump(None), None)

    def test_int(self):
        self.assertEqual(ucl.dump({ "a" : 1 }), "a = 1;\n")

    def test_nested_int(self):
        self.assertEqual(ucl.dump({ "a" : { "b" : 1 } }), "a {\n    b = 1;\n}\n")

    def test_int_array(self):
        self.assertEqual(ucl.dump({ "a" : [1,2,3,4]}), "a [\n    1,\n    2,\n    3,\n    4,\n]\n")

    def test_str(self):
        self.assertEqual(ucl.dump({"a" : "b"}), "a = \"b\";\n")

    def test_unicode(self):
        self.assertEqual(ucl.dump({u"a" : u"b"}), u"a = \"b\";\n")

    def test_float(self):
        self.assertEqual(ucl.dump({"a" : 1.1}), "a = 1.100000;\n")

    def test_boolean(self):
        totest = {"a" : True, "b" : False}
        correct = ["a = true;\nb = false;\n", "b = false;\na = true;\n"]
        self.assertIn(ucl.dump(totest), correct)

    def test_empty_ucl(self):
        self.assertEqual(ucl.dump({}), "")

    def test_json(self):
        totest = { "a" : 1, "b": "bleh;" }
        correct = ['{\n    "a": 1,\n    "b": "bleh;"\n}',
                   '{\n    "b": "bleh;",\n    "a": 1\n}']
        self.assertIn(ucl.dump(totest, ucl.UCL_EMIT_JSON), correct)


if __name__ == '__main__':
    unittest.main()
