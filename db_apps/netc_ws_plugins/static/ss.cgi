#!/bin/bash

if [ x"$SSCGI_NO_FULL" != x"YES" ]; then
  echo "Content-type: text/html"
  echo ""

  echo '<html>'
  echo '<head>'
  echo '<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">'
  echo '<title>Form Example</title>'
  cat <<STYLE_EOF
<style type="text/css">
  html,
  body {
      margin: 0;
      padding: 0;
      border: none;
      text-align: center;
      font-family: 'Open Sans';
  }

  h1,
  h2,
  h3,
  h4,
  h5,
  h6 {
      font-family: 'Roboto', sans-serif;
      font-weight: 700;
  }
  div#testArea {
      display: flex;
      justify-content: center;
      flex-flow: row wrap;
  }

  div#select {
      display: flex;
      justify-content: center;
      flex-flow: row wrap;
  }

  a {
      text-decoration: none;
  }

  .button {
      display: inline-block;
      margin: 10px 5px 0 2px;
      padding: 16px 40px;
      border-radius: 5px;
      font-size: 18px;
      border: none;
      background: #34aadc;
      color: white;
      cursor: pointer;
      text-transform: uppercase;
      font-weight: 700;
      font-family: 'Roboto';
  }
</style>
</head>

<body>
<img src="logo.png" alt="Home">
<h1>Configure Test</h1>
<div id="select">
<form method=GET >
 <table>
  <tr><td><label>Action&nbsp;&nbsp;&nbsp;</label> </td><td>
   <select name="action">
    <option value="status">Status</option>
    <option value="start">Start</option>
    <option value="stop">Stop</option>
    <option value="restart">Restart</option>
   </select>
  </td></td></tr>
  <tr><td><label>Bandwidth&nbsp;&nbsp;&nbsp;</label></td><td>
   <input type="text" name="bandw" size=12></td><td>Bits/S</td></tr>
  <tr><td><label>Test Type&nbsp;&nbsp;&nbsp;</label> </td><td>
    <input type="radio" name="ttype" value="iperfserver" checked> Iperf Server<br>
    <input type="radio" name="ttype" value="iperfclient"> Iperf Client<br>
    <input type="radio" name="ttype" value="pktgen"> Pkt Gen (H/S) </td><td></td></tr>
   <tr><td></td><td><input type="submit" value="Send"></td><td></tr>
 </table>
</div>
<p>
STYLE_EOF

fi

term_str() {
  if [ x"$SSCGI_NO_FULL" != x"YES" ]; then
    echo '</p>'
    echo '</body>'
    echo '</html>'
  fi
}

uname -a
echo "<br>"
date
echo '</p>'


if [ "$REQUEST_METHOD" != "GET" ]; then
  echo "<hr><p>Script Error:"\
       "<br>Usage error, cannot complete request, REQUEST_METHOD!=GET."\
       "<br>Check your FORM declaration and be sure to use METHOD=\"GET\".<hr>"
  term_str
  exit 1
fi

# Check search arguments

if [ -z "$QUERY_STRING" ]; then
  term_str
  exit 0
fi

ACTION=`echo "$QUERY_STRING" | sed -n 's/^.*action=\([^&]*\).*$/\1/p' | sed "s/%20/ /g"`
BANDW=`echo "$QUERY_STRING" | sed -n 's/^.*bandw=\([^&]*\).*$/\1/p' | sed "s/%20/ /g"`
TTYPE=`echo "$QUERY_STRING" | sed -n 's/^.*ttype=\([^&]*\).*$/\1/p' | sed "s/%20/ /g"`

CMD=""
SUDO=""
BITPS=""

case $BANDW in
  '')
      BITPS=''
  ;;
  [0-9]*G)
      BITPS=`echo $BANDW | sed -n 's/^\([0-9]*\).*/\1/p'`
      BITPS=`bc <<< "${BITPS} * 1000000000"`
  ;;
  [0-9]*M)
      BITPS=`echo $BANDW | sed -n 's/^\([0-9]*\).*/\1/p'`
      BITPS=`bc <<< "${BITPS} * 1000000"`
  ;;
  [0-9]*K)
      BITPS=`echo $BANDW | sed -n 's/^\([0-9]*\).*/\1/p'`
      BITPS=`bc <<< "${BITPS} * 1000"`
  ;;
  [0-9]*)
      BITPS="${BANDW}"
  ;;
  *)
      BITPS=''
  ;;
esac

if [ -n "$BITPS" ]; then
  if [ "$BITPS" -le "0" ]; then
   echo "<hr><p>Error:"\
       "<br>Usage error, bandwidth calculated to be zero."
   term_str
   exit 1
  fi
fi

case $TTYPE in
  iperfserver)
   NAME="Iperf Server"
  ;;
  iperfclient)
    NAME="Iperf Client"
  ;;
  pktgen)
    NAME="Pkt Gen"
  ;;
  *)
    echo "<hr>Script Error:"\
       "<br>Usage error, cannot complete request, ttype radio button not set to valid value."
    term_str
    exit 1
esac

PIDFILE=$TTYPE.pid

EXEC_PATH="share/lwsws/netc_netstat/"

IS_RUNNING=''
check_running() {
    IS_RUNNING=''
    if [ -f $PIDFILE ]; then
        PID=`cat $PIDFILE`
        CMDLINE=`ps axf | grep ${PID} | grep -v grep`
        if [ -n "$CMDLINE" ]; then
          IS_RUNNING='yes'
        fi
    fi
}

get_status() {
    echo "<p>Checking $NAME<br>"
    if [ -f $PIDFILE ]; then
        PID=`cat $PIDFILE`
        CMDLINE=`ps axjf | grep ${PID} | grep -v grep`
        if [ -z "$CMDLINE" ]; then
          echo "Process dead but pidfile exists"
          rm -f $PIDFILE
        else
          echo "Running : ${CMDLINE}"
        fi
    else
        echo "Service not running"
    fi
}

do_start() {
    echo "<p>Starting $NAME<br>"
    PID=`(set -m; exec ${EXEC_PATH}${CMD}) > /dev/null 2>&1 & echo $!`
    #echo "Saving PID" $PID " to " $PIDFILE
    if [ -z $PID ]; then
        echo "Start Failed"
    else
        echo $PID > $PIDFILE
        echo "Start Ok"
    fi
}

list_descendants() {
  local children=$(ps -o pid= --ppid "$1")
  for pid in $children
  do
    list_descendants "$pid"
  done

  echo "$children"
}

do_stop() {
    echo "<p>Stopping $NAME<br>"
    if [ -f $PIDFILE ]; then
        PID=`cat $PIDFILE`
        local LIST_DESC=`list_descendants $PID`
	# magic to kill whole process group
        # sudo kill -- -$(ps -o pgid= $PID | grep -o '[0-9]*')
        sudo kill $PID
        rm -f $PIDFILE
        if [ -n "$LIST_DESC" ]; then
            sudo kill $LIST_DESC
        fi
        echo "Stopped Ok"
    else
        echo "Not running (pidfile not found)"
    fi

}

do_param() {
  case "$TTYPE" in
  iperfserver)
   CMD="iperfserver.sh"
  ;;
  iperfclient)
    if [ -z $BITPS ]; then
      echo "<hr><p>Error:"\
       "<br>Usage error, whole number bandwidth required."
      term_str
      exit 1
    fi
    CMD="iperfclient.sh $BITPS"
  ;;
  pktgen)
    if [ -z $BITPS ]; then
      echo "<hr><p>Error:"\
       "<br>Usage error, whole number bandwidth required."
      term_str
      exit 1
    fi
    CMD="pktgen.sh ${BITPS}"
  ;;
  esac
}

case "$ACTION" in
  start)
    do_param
    check_running
    if [ -n "$IS_RUNNING" ]; then
      get_status
    else
      do_start
    fi
  ;;
  status)
    get_status
  ;;
  stop)
    do_stop
  ;;

  restart)
    do_param
    check_running
    if [ -n "$IS_RUNNING" ]; then
      do_stop
      sleep 1
      echo "<br>"
    fi
    check_running
    if [ -n "$IS_RUNNING" ]; then
      get_status
    else
      do_start
    fi
  ;;

  *)
    get_status
esac

term_str

exit 0


