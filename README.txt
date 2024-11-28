test00 : The sleep start and end times are not exact, but the processes sleep for the correct amount of time.

test01 : The processes do not wake up in the same order as the example output, but they all sleep for the correct
    amount of time and wake up at the correct time.

test02 : Same as test00--the reported times are not exact, but they still sleep for the right amount of time.

test07 : Processes do not report being done writing in the same order as the example output, but the terminal
    outputs match the example, indicating that the writes were indeed done in the correct order.

