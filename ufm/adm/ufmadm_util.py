
def print_switch_result(response,
                        expected_cmd,
                        success_print,
                        failure_print):

    for item in response['results']:
        if item['executed_command'] == expected_cmd:
            if item['status'] == 'OK':
                print(success_print)
                return True
            else:
                print(failure_print)
                print('cmd: {}'.format(item['executed_command']))
                print('status: {}'.format(item['status']))
                print('status_message: {}'.format(item['status_message']))
                return False


