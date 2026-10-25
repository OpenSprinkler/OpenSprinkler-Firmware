import unittest

from tests.check_esp8266_stack import check_report


class StackGuardTest(unittest.TestCase):
    def test_current_and_limit(self):
        for size in (304, 330):
            self.assertEqual(check_report(f"main.cpp:692:6:void do_loop()\t{size}\tdynamic\n"), size)

    def test_regression(self):
        for size in (331, 592):
            with self.assertRaisesRegex(ValueError, "limit"):
                check_report(f"main.cpp:682:6:void do_loop()\t{size}\tstatic\n")

    def test_missing_duplicate_or_malformed(self):
        entry = "main.cpp:692:6:void do_loop()\t304\tdynamic\n"
        for report in ("", "main.cpp:1:6:void other()\t16\tstatic", entry + entry,
                       "main.cpp:692:6:void do_loop()\tunknown\tdynamic"):
            with self.assertRaises(ValueError):
                check_report(report)


if __name__ == "__main__":
    unittest.main()
