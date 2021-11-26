#!/bin/bash
#
# Examples of using the Mock RDB for unit test:
#
#

#
# test the application "rdb_tool" by using the mock RDB library
#

#
# usage guild line
#
print_usage()
{
    cat << EOF

test.sh

description:
    Implement the unit test for rdb_tool (rdb_get, rdb_set, rdb_del, rdb_wait and rdb_set_wait)

usage:
    test.sh <app_name>

input:
    app_name:   'rdb_get', 'rdb_set', 'rdb_del', 'rdb_wait' or 'rdb_set_wait'
                or 'rdb_tool' for testing all applications (no single quotes).

EOF
}


#
# check if the application is based on the Mock RDB
#
check_lib_usage()
{
    cd ./test/

    cmd_to_check=$1
    #check_output=$(nm $cmd_to_check)
    check_output=$(objdump -R $cmd_to_check)

    echo "Checking if the command '"$cmd_to_check"' is based on the Mock RDB library..."

    #if [[ "$check_output" == *'search_insert_pos'* ]]
    if [[ "$check_output" == *'__progname'* ]]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    cd ..
}


#
# backup original logs before test
#
backup_log()
{
    mkdir $rdb_log_bck > /dev/null 2>&1
    mv -f ${rdb_log_dir}* $rdb_log_bck > /dev/null 2>&1
}


#
# recover original logs after test
#
recover_log()
{
    mv -f ${rdb_log_bck}* $rdb_log_dir > /dev/null 2>&1
    rm -R $rdb_log_bck > /dev/null 2>&1
}


#
# backup original mock driver file before test
#
backup_mrdb()
{
    mkdir $rdb_fl_bck > /dev/null 2>&1
    mv -f $rdb_fl $rdb_fl_bck > /dev/null 2>&1
    cp -f ./test/mock_rdb_info $rdb_fl > /dev/null 2>&1
}


#
# recover original mock driver file before test
#
recover_mrdb()
{
    mv -f ${rdb_fl_bck}.rdb_info $rdb_fl > /dev/null 2>&1
    rm -R $rdb_fl_bck > /dev/null 2>&1
    rm test.log > /dev/null 2>&1
    rm test.output > /dev/null 2>&1
}


#
# test invalid input
#
test_rdb_tool_inv_input()
{
    sub_rdb_tool=$1
    backup_log

    get_output=$(./$sub_rdb_tool -h 2>&1)
    get_op_len=${#get_output}
    echo "$get_output" > test.output

    if [ $(grep 'Usage: function \[options\] \[arguments\]...' test.output | wc -l) -eq 1 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
}


#
# test rdb_get by various testing cases
#
test_rdb_get()
{
    cd ./test/
    # testing case 1: no input
    echo "'rdb_get' testing case 1: no input ..."
    backup_log

    get_output=$(./rdb_get)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log

    #
    # analyse the output and log file to see if the program works well
    # only the testing results with correct output and logs are PASSED
    # may use more accurate/complex regular expressions or matching conditions to check the Mock RDB log
    # similar changes may applied to other testing cases
    #
    if [ $(grep rdb_get test.log | wc -l) -eq 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
    

    # testing case 2: invalid option -h
    echo "'rdb_get' testing case 2: invalid option '-h' ..."
    test_rdb_tool_inv_input rdb_get


    # testing case 3: option -l
    echo "'rdb_get' testing case 3: option '-l' ..."
    backup_log

    get_output=$(./rdb_get -l 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable (null) get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^admin.' test.output | wc -l) -ge 1 -a \
            $(grep '^mock_rdb.test.variable' test.output | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
        


    # testing case 4: option -l <part name of an existing variable>
    echo "'rdb_get' testing case 4: option '-l dmin' ..."
    backup_log

    get_output=$(./rdb_get -l dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable dmin get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^admin.' test.output | wc -l) -ge 1 -a \
            $(grep '^mock_rdb.test.variable' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 5: option -l <full name of an existing variable>
    echo "'rdb_get' testing case 5: option '-l mock_rdb.test.variable' ..."
    backup_log

    get_output=$(./rdb_get -l mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'mock_rdb.test.variable' test.output | wc -l) -eq 1 -a \
            $(grep '^admin' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 6: option -l <name of an non-exist variable>
    echo "'rdb_get' testing case 6: option '-l no_such_var' ..."
    backup_log

    get_output=$(./rdb_get -l no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get names failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 8: option -L
    echo "'rdb_get' testing case 8: option '-L' ..."
    backup_log

    get_output=$(./rdb_get -L 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable (null) get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^admin.' test.output | wc -l) -ge 1 -a \
            $(grep '^mock_rdb.test.variable value_for_test' test.output | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 9: option -L <part name of an existing variable>
    echo "'rdb_get' testing case 9: option '-L dmin' ..."
    backup_log

    get_output=$(./rdb_get -L dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable dmin get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable admin.firewall.enable get successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^admin.' test.output | wc -l) -ge 1 -a \
            $(grep '^admin.firewall.enable 1' test.output | wc -l) -eq 1 -a \
            $(grep '^mock_rdb.test.variable' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 10: option -L <full name of an existing variable>
    echo "'rdb_get' testing case 10: option '-L mock_rdb.test.variable' ..."
    backup_log

    get_output=$(./rdb_get -L mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'mock_rdb.test.variable value_for_test' test.output | wc -l) -eq 1 -a \
            $(grep '^admin' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log



    # testing case 11: option -L <name of an non-exist variable>
    echo "'rdb_get' testing case 11: option '-L no_such_var' ..."
    backup_log

    get_output=$(./rdb_get -L no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get names failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 12: option -f
    echo "'rdb_get' testing case 12: option '-f' ..."
    backup_log

    get_output=$(./rdb_get -f 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^failed to get flags' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 13: option -f <part name of an existing variable>
    echo "'rdb_get' testing case 13: option '-f dmin' ..."
    backup_log

    get_output=$(./rdb_get -f dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable dmin get info failed' test.log | wc -l) -eq 1 -a \
            $(grep '^failed to get flags' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 14: option -f <full name of an existing variable>
    echo "'rdb_get' testing case 14: option '-f mock_rdb.test.variable' ..."
    backup_log

    get_output=$(./rdb_get -f mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '0x0020' test.output | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log

    
    # testing case 15: option -f <name of an non-exist variable>
    echo "'rdb_get' testing case 15: option '-f no_such_var' ..."
    backup_log

    get_output=$(./rdb_get -f no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get info failed' test.log | wc -l) -eq 1 -a \
            $(grep '^failed to get flags' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
    
    
    # testing case 16: option -P
    echo "'rdb_get' testing case 16: option '-P' ..."
    backup_log

    get_output=$(./rdb_get -P 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^failed to get permissions' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 17: option -P <part name of an existing variable>
    echo "'rdb_get' testing case 17: option '-P dmin' ..."
    backup_log

    get_output=$(./rdb_get -P dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable dmin get info failed' test.log | wc -l) -eq 1 -a \
            $(grep '^failed to get permissions' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 18: option -P <full name of an existing variable>
    echo "'rdb_get' testing case 18: option '-P mock_rdb.test.variable' ..."
    backup_log

    get_output=$(./rdb_get -P mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '0x0fff' test.output | wc -l) -eq 1 -a \
            $(grep '0x00' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 19: option -P <name of an non-exist variable>
    echo "'rdb_get' testing case 19: option '-P no_such_var' ..."
    backup_log

    get_output=$(./rdb_get -P no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get info failed' test.log | wc -l) -eq 1 -a \
            $(grep '^failed to get permissions' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 20: option -a
    echo "'rdb_get' testing case 20: option '-a' ..."
    backup_log

    get_output=$(./rdb_get -a 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable (null) get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'admin.' test.output | wc -l) -ge 1 -a \
            $(grep '0x0020 0x0fff mock_rdb.test.variable' test.output | wc -l) -eq 1 -a \
            $(grep '^0x' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 21: option -a <part name of an existing variable>
    echo "'rdb_get' testing case 21: option '-a dmin' ..."
    backup_log

    get_output=$(./rdb_get -a dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable dmin get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'admin.' test.output | wc -l) -ge 1 -a \
            $(grep '0x0020 0x0000 admin.firewall.enable' test.output | wc -l) -ge 1 -a \
            $(grep 'mock_rdb.test.variable' test.output | wc -l) -eq 0 -a \
            $(grep '^0x' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 22: option -a <full name of an existing variable>
    echo "'rdb_get' testing case 22: option '-a mock_rdb.test.variable' ..."
    backup_log

    get_output=$(./rdb_get -a mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^0x0020 0x0fff mock_rdb.test.variable' test.output | wc -l) -eq 1 -a \
            $(grep 'admin' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 23: option -a <name of an non-exist variable>
    echo "'rdb_get' testing case 23: option '-a no_such_var' ..."
    backup_log

    get_output=$(./rdb_get -a no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get names failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 24: option -A
    echo "'rdb_get' testing case 24: option '-A' ..."
    backup_log

    get_output=$(./rdb_get -A 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable (null) get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'admin.' test.output | wc -l) -ge 1 -a \
            $(grep '0x0020 0x0fff mock_rdb.test.variable value_for_test' test.output | wc -l) -eq 1 -a \
            $(grep '^0x' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 25: option -A <part name of an existing variable>
    echo "'rdb_get' testing case 25: option '-A dmin' ..."
    backup_log

    get_output=$(./rdb_get -A dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable dmin get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable admin.firewall.enable get successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'admin.' test.output | wc -l) -ge 1 -a \
            $(grep '0x0020 0x0000 admin.firewall.enable 1' test.output | wc -l) -ge 1 -a \
            $(grep 'mock_rdb.test.variable' test.output | wc -l) -eq 0 -a \
            $(grep '^0x' test.output | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
           

    # testing case 26: option -A <full name of an existing variable>
    echo "'rdb_get' testing case 26: option '-A mock_rdb.test.variable' ..."
    backup_log

    get_output=$(./rdb_get -A mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get names successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable get successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^0x0020 0x0fff mock_rdb.test.variable value_for_test' test.output | wc -l) -eq 1 -a \
            $(grep 'admin' test.output | wc -l) -eq 0 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -ne 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 27: option -A <name of an non-exist variable>
    echo "'rdb_get' testing case 27: option '-A no_such_var' ..."
    backup_log

    get_output=$(./rdb_get -A no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log

    if [ $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get names failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
    echo

    #
    # more testing cases may required.
    #

    cd ..
}


#
# test rdb_set by several testing cases
#
test_rdb_set()
{
    cd ./test/
    # testing case 1: no input
    echo "'rdb_set' testing case 1: no input ..."
    backup_log

    get_output=$(./rdb_set 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set test.log | wc -l) -eq 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^failed to set' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 2: invalid option -h
    echo "'rdb_set' testing case 2: invalid option '-h' ..."
    test_rdb_tool_inv_input rdb_set


    # testing case 3: <name of an existing variable> <no value>
    echo "'rdb_set' testing case 3: mock_rdb.test.variable ..."
    backup_log

    get_output=$(./rdb_set mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable update successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        get_output=$(./rdb_get mock_rdb.test.variable 2>&1)
        get_op_len=${#get_output}

        if [ $get_op_len -eq 0 ]
        then
            echo "PASSED"
        else
            test_rslt=0
            echo "FAILED"
        fi
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 4: <name of an existing variable> <new value>
    echo "'rdb_set' testing case 4: mock_rdb.test.variable new_set_val ..."
    backup_log

    get_output=$(./rdb_set mock_rdb.test.variable new_set_val 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable update successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        # get variable value to check if the set is OK
        get_output=$(./rdb_get mock_rdb.test.variable 2>&1)
        get_op_len=${#get_output}
        echo "$get_output" > test.output

        if [ $(grep '^new_set_val' test.output | wc -l) -eq 1 -a \
                $get_op_len -eq 11 ]
        then
            echo "PASSED"
        else
            test_rslt=0
            echo "FAILED"
        fi
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 5: <name of non-exist variable> <new value>
    echo "'rdb_set' testing case 5: mock_rdb.test.variable.two new_set_val ..."
    backup_log

    get_output=$(./rdb_set mock_rdb.test.variable.two new_set_val 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable.two update successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        # get variable value to check if the set is OK
        get_output=$(./rdb_get mock_rdb.test.variable 2>&1)
        get_op_len=${#get_output}
        echo "$get_output" > test.output

        if [ $(grep '^new_set_val' test.output | wc -l) -eq 1 -a \
                $get_op_len -eq 11 ]
        then
            echo "PASSED"
        else
            test_rslt=0
            echo "FAILED"
        fi
    else
        test_rslt=0
        echo "FAILED"
    fi

    cp -f mock_rdb_info $rdb_fl > /dev/null 2>&1
    recover_log
    echo

    #
    # more testing cases may required.
    #

    cd ..
}


#
# test rdb_del by several testing cases
#
test_rdb_del()
{
    cd ./test/

    # testing case 1: no input
    echo "'rdb_del' testing case 1: no input ..."
    backup_log

    get_output=$(./rdb_del 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_del test.log | wc -l) -eq 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^(null) does not exist' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 2: invalid option -h
    echo "'rdb_del' testing case 2: invalid option '-h' ..."
    test_rdb_tool_inv_input rdb_del


    # testing case 3: <part name of an existing variable>
    echo "'rdb_del' testing case 3: admin ..."
    backup_log

    get_output=$(./rdb_del admin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_del test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable admin get info failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^admin does not exist' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 4: <name of an existing variable>
    echo "'rdb_del' testing case 4: mock_rdb.test.variable ..."
    backup_log

    get_output=$(./rdb_del mock_rdb.test.variable 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_del test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable delete successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^mock_rdb.test.variable deleted' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    cp -f mock_rdb_info $rdb_fl > /dev/null 2>&1
    recover_log


    # testing case 5: <name of an non-exist variable>
    echo "'rdb_del' testing case 5: no_such_var ..."
    backup_log

    get_output=$(./rdb_del no_such_var 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_del test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable no_such_var get info failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^no_such_var does not exist' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 6: delete all variables (option -A, -a, -L or -l)
    echo "'rdb_del' testing case 6: option -A, -a, -L or -l ..."
    backup_log

    get_output=$(./rdb_del -A 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_del test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'delete successfully.' test.log | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep 'deleted' test.output | wc -l) -ge 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    cp -f mock_rdb_info $rdb_fl > /dev/null 2>&1
    recover_log


    # testing case 7: delete multiple variables (option -A, -a, -L or -l <part name of existing variables>)
    echo "'rdb_del' testing case 7: option -A, -a, -L or -l dmin ..."
    backup_log

    get_output=$(./rdb_del -A dmin 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_del test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'delete successfully.' test.log | wc -l) -ge 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep 'deleted' test.output | wc -l) -ge 1 -a \
            $(grep '^admin.firewall.enable deleted' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    cp -f mock_rdb_info $rdb_fl > /dev/null 2>&1
    recover_log
    echo

    #
    # more testing cases may required.
    #

    cd ..
}


#
# test rdb_wait by several testing cases
#
test_rdb_wait()
{
    cd ./test/

    # testing case 1: no input
    echo "'rdb_wait' testing case 1: no input ..."
    backup_log

    get_output=$(./rdb_wait 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_wait test.log | wc -l) -eq 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^variable name not specified' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 2: invalid option -h
    echo "'rdb_wait' testing case 2: invalid option '-h' ..."
    test_rdb_tool_inv_input rdb_wait


    # testing case 3: <name of non-exist variable> or <part name of existing variable>
    echo "'rdb_wait' testing case 3: mock_rdb ..."
    backup_log

    get_output=$(./rdb_wait mock_rdb 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_wait test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb subscribed failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^failed to subscribe mock_rdb - No such file or directory' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 4: <name of existing variable> <short time can be easily expired>
    echo "'rdb_wait' testing case 4: mock_rdb.test.variable 5 ..."
    backup_log

    get_output=$(./rdb_wait mock_rdb.test.variable 5 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_wait test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb.test.variable subscribed successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 5: <name of existing variable> <long time will not be easily expired before the signal>
    # so needs a correct rdb_set to make it quit
    echo "'rdb_wait' testing case 5: mock_rdb.test.variable ..."
    backup_log

    # running the rdb_wait in the background, waiting for variable changes
    ./rdb_wait mock_rdb.test.variable &
    # wait for a few seconds to change the variable
    sleep 5
    ./rdb_set admin.firewall.enable 0 2>&1
    get_output=$(ps -l 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output
    

    # check if rdb_wait is still alive
    if [ $(grep rdb_wait test.log | wc -l) -ge 2 -a \
            $(grep rdb_set test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 2 -a \
            $(grep '^variable mock_rdb.test.variable subscribed successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'rdb_wait' test.output | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -gt 0 ]
    then
        #
        # now, the rdb_wait is still alive after ./rdb_set admin.firewall.enable 0
        # so set the variable to make rdb_wait quit
        #
        ./rdb_set mock_rdb.test.variable value_to_quit_wait
        get_output=$(ps -l 2>&1)
        get_op_len=${#get_output}
        cat ${rdb_log_dir}20*.log > test.log
        echo "$get_output" > test.output

        if [ $(grep rdb_wait test.log | wc -l) -ge 2 -a \
                $(grep rdb_set test.log | wc -l) -ge 2 -a \
                $(grep '^Mock RDB closed' test.log | wc -l) -eq 3 -a \
                $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 3 -a \
                $(grep '^variable mock_rdb.test.variable subscribed successfully' test.log | wc -l) -eq 1 -a \
                $(grep '^send subscribed signal' test.log | wc -l) -ge 1 -a \
                $(grep 'rdb_wait' test.output | wc -l) -eq 0 -a \
                $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
                $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
                $get_op_len -gt 0 ]
        then
            echo "PASSED"
        else
            test_rslt=0
            echo "FAILED"
        fi
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
    echo

    #
    # more testing cases may required.
    #

    cd ..
}


#
# test rdb_set_wait by several testing cases
#
test_rdb_set_wait()
{
    cd ./test/

    # testing case 1: no input
    echo "'rdb_set_wait' testing case 1: no input ..."
    backup_log

    get_output=$(./rdb_set_wait 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set_wait test.log | wc -l) -eq 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^variable name not specified' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 2: invalid option -h
    echo "'rdb_set_wait' testing case 2: invalid option '-h' ..."
    test_rdb_tool_inv_input rdb_set_wait


    # testing case 3: <name of non-exist variable> or <part name of existing variable>
    echo "'rdb_set_wait' testing case 3: mock_rdb ..."
    backup_log

    get_output=$(./rdb_set_wait mock_rdb 10 mock_rdb.test.variable val_set_to_wait 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set_wait test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^variable mock_rdb subscribed failed' test.log | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -ge 1 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $(grep '^failed to subscribe mock_rdb - No such file or directory' test.output | wc -l) -eq 1 -a \
            $get_op_len -gt 0 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 4: <name of existing variable> <short time can be easily expired>
    echo "'rdb_set_wait' testing case 4: mock_rdb.test.variable 5 mock_rdb.test.variable.two val_set_to_wait ..."
    backup_log

    ./rdb_set_wait mock_rdb.test.variable 5 mock_rdb.test.variable.two val_set_to_wait 2>&1
    get_output=$(./rdb_get mock_rdb.test.variable.two 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    if [ $(grep rdb_set_wait test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 2 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 2 -a \
            $(grep '^variable mock_rdb.test.variable subscribed successfully' test.log | wc -l) -eq 1 -a \
            $(grep '^val_set_to_wait' test.output | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -eq 15 ]
    then
        echo "PASSED"
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log


    # testing case 5: <name of existing variable> <long time will not be easily expired before the signal>
    # so needs a correct rdb_set to make it quit
    echo "'rdb_set_wait' testing case 5: mock_rdb.test.variable 10000 mock_rdb.test.variable.two val_set_to_wait ..."
    backup_log

    # running the rdb_wait in the background, waiting for variable changes
    ./rdb_set_wait mock_rdb.test.variable 10000 mock_rdb.test.variable.two val_set_to_wait &
    # wait for a few seconds to change the variable
    sleep 5
    ./rdb_set admin.firewall.enable 0 2>&1
    get_output=$(ps -l 2>&1)
    get_op_len=${#get_output}
    cat ${rdb_log_dir}20*.log > test.log
    echo "$get_output" > test.output

    # check if rdb_wait is still alive
    if [ $(grep rdb_set_wait test.log | wc -l) -ge 2 -a \
            $(grep rdb_set test.log | wc -l) -ge 2 -a \
            $(grep '^Mock RDB closed' test.log | wc -l) -eq 1 -a \
            $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 2 -a \
            $(grep '^variable mock_rdb.test.variable subscribed successfully' test.log | wc -l) -eq 1 -a \
            $(grep 'rdb_set_wait' test.output | wc -l) -eq 1 -a \
            $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
            $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
            $get_op_len -gt 0 ]
    then
        #
        # now, the rdb_wait is still alive after ./rdb_set admin.firewall.enable 0
        # so set the variable to make rdb_wait quit
        #
        ./rdb_set mock_rdb.test.variable value_to_quit_wait
        get_output=$(ps -l 2>&1)
        get_op_len=${#get_output}
        cat ${rdb_log_dir}20*.log > test.log
        echo "$get_output" > test.output

        if [ $(grep rdb_set_wait test.log | wc -l) -ge 2 -a \
                $(grep rdb_set test.log | wc -l) -ge 2 -a \
                $(grep '^Mock RDB closed' test.log | wc -l) -eq 3 -a \
                $(grep '^Mock RDB opened successfully' test.log | wc -l) -eq 3 -a \
                $(grep '^variable mock_rdb.test.variable subscribed successfully' test.log | wc -l) -eq 1 -a \
                $(grep '^send subscribed signal' test.log | wc -l) -ge 1 -a \
                $(grep 'rdb_set_wait' test.output | wc -l) -eq 0 -a \
                $(grep '&&&&&&&&&&&&&&&' test.log | wc -l) -eq 0 -a \
                $(grep '@@@@@@@@@@@@@@@' test.log | wc -l) -eq 0 -a \
                $get_op_len -gt 0 ]
        then
            get_output=$(./rdb_get mock_rdb.test.variable.two 2>&1)

            if [ $get_output = 'val_set_to_wait' ]
            then
                echo "PASSED"
            else
                test_rslt=0
                echo "FAILED"
            fi
        else
            test_rslt=0
            echo "FAILED"
        fi
    else
        test_rslt=0
        echo "FAILED"
    fi

    recover_log
    echo

    #
    # more testing cases may required.
    #

    cd ..
}



############################
# start of the main script #
############################

# no input argument
if [ $# -lt 1 ]
then
    cmd_to_test='rdb_tool' # default to testing everything
else
	cmd_to_test=$1
fi

# set directories and files for the test
test_rslt=1
rdb_fl_dir=~/.mock_rdb/
rdb_fl_bck=${rdb_fl_dir}test_backup/
rdb_fl=${rdb_fl_dir}.rdb_info
rdb_log_dir=${rdb_fl_dir}rdb_log/
rdb_log_bck=${rdb_log_dir}test_backup/

# back up driver file for the test first
backup_mrdb


# test all the commands of rdb_tool
if [ $cmd_to_test = 'rdb_tool' ]
then
    # check if the Mock RDB library is used
    echo "Testing all commands in '$cmd_to_test'..."
    check_lib_usage 'rdb_get'
    check_lib_usage 'rdb_set'
    check_lib_usage 'rdb_del'
    check_lib_usage 'rdb_wait'
    check_lib_usage 'rdb_set_wait'

    # Mock RDB library is not in use
    if [ $test_rslt -eq 0 ]
    then
        recover_mrdb
        echo
        echo "The unit test of '$cmd_to_test' is FAILED..."
        exit -1;
    fi

    # test all the commands
    # add more testing cases to corresponding functions if necessary
    echo
    echo "Starting the tests of each command..."
    test_rdb_get
    test_rdb_set
    test_rdb_del
    test_rdb_wait
    test_rdb_set_wait

# test individual the commands of rdb_tool
else
    # check if the Mock RDB library is used
	echo "Testing command '$cmd_to_test'..."
	check_lib_usage $cmd_to_test

	# Mock RDB library is not in use
    if [ $test_rslt -eq 0 ]
    then
        recover_mrdb
        echo
        echo "The unit test of '$cmd_to_test' is FAILED..."
        exit -1;
    fi

	# test individual the command
	case $cmd_to_test in
	    'rdb_get')
	       test_rdb_get
	       ;;
	    'rdb_set')
	       test_rdb_set
	       ;;
	    'rdb_del')
	       test_rdb_del
	       ;;
	    'rdb_wait')
	       test_rdb_wait
	       ;;
	    'rdb_set_wait')
           test_rdb_set_wait
           ;;
	    *)
	       print_usage
	       ;;
	esac
fi


# recover scene after the test
recover_mrdb
echo

# check if the test is PASSED or FAILED
if [ $test_rslt -eq 1 ]
then
    echo "The unit test for '$cmd_to_test' is PASSED..."
    exit 0;
else
    echo "The unit test of '$cmd_to_test' is FAILED..."
    exit -1;
fi


