#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
import os
import urllib
from xml.etree import ElementTree
from collections import namedtuple

AggregateResult = namedtuple("AggregateResult", ("tests", "failures", "errors", "success_rate", "duration"))


def clean_timestamp(s):
    s = s.replace(':', '_')
    s = s.replace('.', '_')
    return s


def xml_contains_failure(xml_filename):
    try:
        with open(xml_filename, 'r') as fx:
            tree = ElementTree.parse(fx)

        testsuites = tree.getroot()
        if int(testsuites.attrib['failures']) + int(testsuites.attrib['errors']) > 0:
            return True
    except:
        pass

    return False


def aggregate_module(xml_path):
    """ Compute the aggregate test results for a single module's output.
    :param xml_path:
    :return: AggregateResult
    """
    try:
        with open(xml_path, 'r') as fx:
            tree = ElementTree.parse(fx)

    except IOError:
        return AggregateResult(tests=0, failures=0, errors=1, success_rate=0.0, duration=0.0)

    testsuites = tree.getroot()
    if testsuites is None:
        return None

    tests = int(testsuites.attrib.get('tests'))
    failures = int(testsuites.attrib.get('failures'))
    errors = int(testsuites.attrib.get('errors'))
    duration = float(testsuites.attrib.get('time'))
    success_rate = (tests-failures)/float(tests) if tests else 0.0
    return AggregateResult(tests=tests, failures=failures, errors=errors, success_rate=success_rate, duration=duration)


def aggregate_testsuite(testsuite):
    """ Compute aggregate results for a single test suite (ElemTree node)
    :param testsuite: ElemTree XML node for a testsuite
    :return: AggregateResult
    """
    if testsuite is None:
        return None

    tests = int(testsuite.attrib.get('tests'))
    failures = int(testsuite.attrib.get('failures'))
    errors = int(testsuite.attrib.get('errors'))
    duration = float(testsuite.attrib.get('time'))
    success_rate = (tests-failures)/float(tests) if tests else 0.0
    return AggregateResult(tests=tests, failures=failures, errors=errors, success_rate=success_rate, duration=duration)


def aggregate_results(scan_results):
    """ Compute aggregate results for all of the test runs.
    :param scan_results:
    :return: AggregateResult
    """
    tests = 0
    failures = 0
    errors = 0
    duration = 0.0

    for result in scan_results:
        if result.return_code not in [0, 1]:
            errors += 1
            continue

        module_agg = aggregate_module(result.xml_path)
        if module_agg:
            tests += module_agg.tests
            failures += module_agg.failures
            errors += module_agg.errors
            duration += module_agg.duration
        else:
            errors += 1

    success_rate = (tests-failures)/float(tests) if tests else 0.0
    return AggregateResult(tests=tests, failures=failures, errors=errors, success_rate=success_rate, duration=duration)


def get_stylesheet():
    """ Get the inline stylesheet used for the report. """

    css = """
    .fail {
       background-color:orange;
    }
    .error {
        background-color:red;
    }
    .module_stub {
        background-color:magenta;
    }

    table {
        border: 1px solid black;
    }

    tr.header {
        background-color:#AAA;
    }
    tr.testsuite {
       background-color:#CCC;
    }

    td {
        padding: 5px;
        border: 1px solid black;
    }
    td.testcase {
       padding-left: 50px;
    }

    div.module {
        margin-top: 20px;
        margin-bottom: 5px;
        font-weight: bold;
        font-size: 20px;
    }
    """
    return css


def write_module(scan_result, f, failures_only):
    """ Write HTML report for a single module.
    :param scan_result: ScanResult for module
    :param f: open file handle to write report
    :param failures_only: When True, only writes failures. Otherwise, writes all results.
    """
    def has_failures(testcase):
        failures = testcase.findall('failure')
        return len(failures) > 0

    def newline_to_br(s):
        x = s.replace("\n", "<br/>")
        return x

    try:
        # skip modules with missing XML files
        if not os.path.isfile(scan_result.xml_path):
            return

        # parse module XML output
        with open(scan_result.xml_path, 'r') as fx:
            tree = ElementTree.parse(fx)

        f.write('<table width="90%">')
        f.write('<tr class="header"><td>Test</td><td>Result</td><td>Duration</td></tr>')

        # test suites
        for testsuite in tree.findall('testsuite'):

            # skip successful test suites if filtering by failures
            agg = aggregate_testsuite(testsuite)
            if failures_only and agg.success_rate >= 1.0:
                continue

            # test cases in the test suite
            f.write('<tr class="testsuite"><td colspan="4">{}</td></tr>'.format(testsuite.attrib.get('name')))
            for testcase in testsuite.findall('testcase'):

                # skip successful test cases if filtering by failures
                fail = has_failures(testcase)
                if failures_only and not fail:
                    continue

                # testcase result
                css_class = ['testcase']
                if fail:
                    css_class += ['fail']

                test_result = {
                    'test': testcase.attrib.get('name'),
                    'result': 'fail' if fail else 'pass',
                    'class': ' '.join(css_class),
                    'duration': float(testcase.attrib.get('time'))*1000.0  # milliseconds
                }
                f.write("""<tr class="{class}">
                    <td class="testcase">{test}</td>
                    <td>{result}</td>
                    <td>{duration} msec</td>
                </tr>""".format(**test_result))

                # failure details
                failures = testcase.findall('failure')
                if failures is not None:
                    for failure in failures:
                        msg = newline_to_br(failure.attrib.get('message'))
                        f.write('<tr><td colspan="4">{}</td></tr>'.format(msg))

                pass  # end failure details
            pass  # end testcase
        pass  # end testsuite

        f.write('</table>')

    except Exception as ex:
        f.write("Error {}".format(ex.message))


def create_html_report(scan_results, output_dir, failures_only=False):
    """ Create an HTML report based on the list of scanner results.
    :param scan_results: iterable of ScanResult
    :param output_dir: output directory of report generation
    :param failures_only: when True, only output failures
    :return: None
    """
    filename = os.path.abspath(os.path.join(output_dir,
                                            "{}report.html".format("failure_" if failures_only else "")))

    css = get_stylesheet()

    with open(filename, 'w') as f:

        def td(s):
            f.write("<td>{}</td>".format(s))

        def tr_from_list(items, classes=None):
            if classes:
                f.write('<tr class="{}">'.format(" ".join(classes)))
            else:
                f.write("<tr>")

            for x in items:
                td(x)
            f.write("</tr>")

        def span(s, classes=None):
            if classes:
                str_classes = " ".join(str(x) for x in classes)
                f.write('<span class="{}">{}</span>'.format(str_classes, s))
            else:
                f.write("<span>{}</td>".format(s))

        def fmt_pct(f):
            return "{0:.2f}".format(f*100.0)

        def generate_anchor_name(path):
            """ Generate an on-page anchor name from the input path. """
            return urllib.quote(path)

        f.write("<html><head><style>{}</style></head><body>\n".format(css))
        f.write("<h1>AzTestScanner Report</h1>")

        # aggregate/summary
        agg = aggregate_results(scan_results)
        if agg:
            f.write("<div><table>")
            tr_from_list(["Tests", "Failures", "Errors", "Success Rate", "Time"], ["header"])
            tr_from_list([agg.tests, agg.failures, agg.errors, fmt_pct(agg.success_rate), agg.duration])
            f.write("</table></div>")

        # module summaries
        f.write("<h2>Module Summary ({} Total)</h2>".format(len(scan_results)))
        f.write("<div><table>")
        tr_from_list(["Module", "Tests", "Failures", "Errors", "Success Rate", "Time"], ["header"])
        for scan_result in scan_results:
            agg = aggregate_module(scan_result.xml_path)

            if failures_only and agg.success_rate >= 1.0:
                continue  # skip successes if failure-only specified

            module = os.path.basename(scan_result.path)

            if agg.tests == 0:
                css_classes = ['error']
            elif agg.tests == 1:
                css_classes = ['module_stub']
            elif agg.success_rate < 1.0:
                css_classes = ['fail']
            else:
                css_classes = []

            link_module = '<a href="#{}">{}</a>'.format(generate_anchor_name(module), module)
            tr_from_list([link_module, agg.tests, agg.failures, agg.errors,
                          fmt_pct(agg.success_rate), agg.duration], css_classes)

        f.write("</table></div>")

        # modules details
        f.write("<h2>Module Details</h2>")
        for scan_result in scan_results:
            css_class = "pass" if scan_result.return_code == 0 else "fail"

            if failures_only and scan_result.return_code == 0:
                continue

            module = os.path.basename(scan_result.path)

            f.write('<div class="module">'.format(css_class))
            f.write('<a name="{}"></a>'.format(generate_anchor_name(module)))
            f.write("<div>")
            span(module, [css_class])
            f.write("</div>")

            # error message, if present
            if scan_result.return_code != 0:
                f.write("<div>{} ({})</div>".format(scan_result.error_msg, scan_result.return_code))

            #
            write_module(scan_result, f, failures_only)

            f.write("</div>")
        f.write("</body></html>\n")


def create_html_failure_report(files, output_dir):
    create_html_report(files, output_dir, True)
