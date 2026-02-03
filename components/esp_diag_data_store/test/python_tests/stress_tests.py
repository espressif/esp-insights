import time
import xmlrunner
import requests
import unittest
import urllib3
import urllib.parse
import sys
import argparse
import json
import glob
import subprocess
import os
from datetime import datetime, timedelta


class TestCaseInsights(unittest.TestCase):
    """
    Python stress tests
    """

    def __init__(self, test_name, **kwargs):
        unittest.TestCase.__init__(self, test_name)

        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        self.username = kwargs.get('username', None)
        self.password = kwargs.get('password', None)
        self.base_uri = kwargs.get('base_uri', None)
        self.diag_uri = kwargs.get('diag_uri', None)
        self.node_id = kwargs.get('node_id', None)
        self.endpoint = kwargs.get('endpoint', '/v1/login')

        login_response = self.login()
        self.access_token = json.loads(login_response.text)["accesstoken"]
        super(unittest.TestCase, self).__init__()

    def login(self):
        """
        Login to Insights to access further APIs
        """
        header = {"content-type": "application/json"}
        uri = self.base_uri + self.endpoint
        body = json.dumps({'user_name': self.username, 'password': self.password})
        response = requests.post(url=uri, data=body, headers=header, verify=False, cookies=None)

        return response

    def test_001_get_crash_count(self):
        """
        Device crash count shall not exceed 5
        - Query diagnostics API every minute for crash count
        - Verify API response status is 200
        - Assert crash count does not exceed 5
        - Repeat for 10 minutes duration

        """
        start_time = time.time()
        header = {"Authorization": self.access_token}
        max_time = 10*60
        max_crash_count = 5
        filt = '[{"f":"Node.ID","o":"keyword","v":["%s"]},{"f":"Type","o":"keyword","v":["crash"]}]' % self.node_id
        encoded_filter = urllib.parse.quote(filt, safe='~@#$&()*!+=:;,?/\'')

        from_ts = round(int(datetime.timestamp(datetime.now() - timedelta(minutes=1))))
        crash_count = 0
        while (time.time() - start_time) < max_time:
            to_ts = round(int(datetime.timestamp(datetime.now())))
            url = self.diag_uri + "/query/filters/suggest?from_ts={}&to_ts={}&filters={}&fieldname=Type". \
                format(from_ts, to_ts, encoded_filter)
            resp_crash_filter = requests.get(url=url, verify=False, headers=header, cookies=None)
            json_resp_crash_filter = json.loads(resp_crash_filter.text)["list"]
            self.assertEqual(resp_crash_filter.status_code, 200,
                             "S001.1 Unexpected status code for {} \n".format(self.node_id))
            # Check if any crash is present
            if len(json.loads(resp_crash_filter.text)["list"]):
                self.assertEqual(json_resp_crash_filter[0]["key"], "crash",
                                 "S001.2 key is not present for {}".format(self.node_id))
                self.assertLessEqual(json_resp_crash_filter[0]["count"], max_crash_count,
                                     "S001.3 Crash count is not same as expected for {}".format(self.node_id))
                crash_count = json_resp_crash_filter[0]["count"]

            # update variable curr_ts after every one minute
            time.sleep(60)

        print("\n Final crash count after 10 minutes is {} ".format(crash_count))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--username', default="" , type=str,
                        help="username for login ")
    parser.add_argument('--password', default="", type=str,
                        help="password for login")
    parser.add_argument('--base_uri', default="", type=str,
                        help="environment login uri on which tests are to be run")
    parser.add_argument('--diag_uri', default="", type=str,
                        help="environment api uri on which tests are to be run")
    parser.add_argument('--node_id', default="", type=str,
                        help="node id on which tests are to be performed")
    parser.add_argument('--output', default=".", type=str,
                        help="output directory for test reports")

    args = parser.parse_args()

    kwargs = {
        "username": args.username,
        "password": args.password,
        "base_uri": args.base_uri,
        "diag_uri": args.diag_uri,
        "node_id": args.node_id,
    }

    # Create output directory if it doesn't exist
    os.makedirs(args.output, exist_ok=True)
    
    suite = unittest.TestSuite()
    suite.addTest(TestCaseInsights('test_001_get_crash_count', **kwargs))
    runner = xmlrunner.XMLTestRunner(output=args.output, verbosity=2)
    result = runner.run(unittest.TestSuite(suite))

    xml_pattern = os.path.join(args.output, "TEST-*.xml")
    xml_files = glob.glob(xml_pattern)
    if xml_files:
        try:
            for xml_file in xml_files:
                html_file = xml_file.replace('.xml', '.html')
                subprocess.run(['junit2html', xml_file, html_file], check=True)
                print(f"Generated: {html_file}")
                os.remove(xml_file)
                print(f"Removed: {xml_file}")
        except Exception as e:
            print(f"Warning: Failed to generate HTML report: {e}")
    else:
        print("No XML reports found to convert")

    # Exit with proper code: 0 for success, 1 for failure
    sys.exit(0 if result.wasSuccessful() else 1)
