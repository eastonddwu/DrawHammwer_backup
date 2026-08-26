#!/bin/bash 

### glob variables
SELF_PATH=$(readlink -f $0)
SELF_DIR=$(dirname $SELF_PATH)

# root of apps/ 
readonly SELF_CONF=${SELF_DIR}/tcm_admin.conf
readonly TCM_HOME=$(cd $SELF_DIR/../../ && pwd)
readonly TCONND_HOME=$TCM_HOME/tconnd
readonly TCENTER_HOME=$TCM_HOME/tcenterd
readonly TCMCENTER_HOME=$TCM_HOME/tcm
readonly TAGENT_HOME=$TCM_HOME/tagent
#jafterwang 2017-11-16
readonly ETH1_IP=$(ifconfig -a|grep eth[0-9]: -C 2|grep -v "127.0.0.1" | awk '/inet / { print $2 }');

readonly TCM_COMP=(tcmcenter tagent tcenterd tconnd tcm)
readonly TCM_CONSOLE_CMD=(help history checkconf refreshbuscfg clearbuschannel list listhost listproc enable disable auto noauto start start starthost stop reload check checkbe checkno createcfg createcfghost pushcfg pushcfghost checkagent kill restart kill9 quit pushcollectconf reloadcollectconf getproccfgorrundata exportdeploy getbuschannelsbyhost getbuschannelsbyproc runshell runshellhost enablecollect disablecollect reopentsm tappctrl signal pushcfgex pushcfgexhost cleartsmshm pushtools dumptcmconfig)
readonly TCM_ADMIN_CMDS=(tcm_init cmd list)

# read configuration
if [[ -r ${SELF_CONF} ]]; then
	. ${SELF_CONF}
else
	echo "${SELF_CONF} is not readable, please check."
	exit 0
fi

tcm_bus_init() {
	cat > ${TCM_HOME}/tools/tbusmgr.xml.auto <<EOF
<?xml version="1.0" encoding="gbk" standalone="yes" ?>
<!-- This file is for tbus routes tool application-->

<TbusGCIM>

  <!-- tagent系统分配的业务ID，不使用tagent系统配置缺省值0  -->
  <BussinessID>0</BussinessID>

  <!-- 十进制点分表示法表示的通信地址模版  -->
  <AddrTemplet>8.8.8.8</AddrTemplet>

   <!-- GCIM共享内存key -->
  <GCIMShmKey>$GCIM_SHM_KEY</GCIMShmKey>

  <Channels>
    <Priority>8</Priority>
    <!-- 通道两端进程的通信地址，使用点分法表示 -->

    <Address>$TCMCENTER_BUS_ID</Address>
    <Address>$TCONND_BUS_ID</Address>

     <!-- 对于出现在配置中的第一个进程而言，SendSize，RecvSize分别表示此进程相关发送，接受 数据队列的大小。
     由于两个进程是对等的，因此第一个进程的发送队列大小(SendSize)就是第二个进程的接受队列大小(RecvSize) -->
    <SendSize>20480000</SendSize>
    <RecvSize>20480000</RecvSize>
    <Desc>tcmcenter and its tconnd</Desc>
  </Channels>

  <Channels>
    <Priority>8</Priority>
    <Address>$TCMCENTER_BUS_ID</Address>
    <Address>$TCENTER_BUS_ID</Address>
    <SendSize>20480000</SendSize>
    <RecvSize>20480000</RecvSize>
    <Desc>tcmcenter and its centerd</Desc>
  </Channels>

</TbusGCIM>
EOF

	# create bus channel for tcm components
	if [[ -s $TCM_HOME/tools/tbusmgr.xml.auto ]]; then
		$TCM_HOME/tools/tbusmgr -C $TCM_HOME/tools/tbusmgr.xml.auto -X
		$TCM_HOME/tools/tbusmgr -C $TCM_HOME/tools/tbusmgr.xml.auto -W
	else
		echo "auto-generate tcm bus relation config file failed."
		exit 1
	fi

	# generate xml config for tcm components
	auto_tconnd_xml
	auto_tcmconsole_xml
	auto_tcenterd_xml
	auto_tagent_xml
	auto_tcmcenter_xml
}

auto_tcenterd_xml() {
	cat > $TCENTER_HOME/cfg/tcenterd.xml.auto <<EOF
<?xml version="1.0" encoding="GBK" standalone="yes" ?>
<tcenterdconf __version="1">

    <ListenAddr>
	<!--配置为实际主机的内网ip-->
        <ip>$ETH1_IP</ip>
        <port>$TCENTER_LISTEN_PORT</port>
    </ListenAddr>

    <MaxFdNum>10000 </MaxFdNum>
    <ReadIdle>30 </ReadIdle>
    <NetMsgSizeLimit>65535 </NetMsgSizeLimit>
    <!--tcenterd父节点的ip地址，一般使用在tdirty业务上,若使用了tdirty，必须把IsMaster配置为0-->
    <ParentAddr>
        <ip>172.25.75.15</ip>
        <port>8899</port>
    </ParentAddr>

    <Location></Location>
	<!--若tcenterd存在父节点，若使用了tdirty脏词功能,IsMaster必须配置为0,否则IsMaster配置为1-->
    <IsMaster>1</IsMaster>

    <BufBlockSize>10 </BufBlockSize>
    <MaxBoclNum>1000 </MaxBoclNum>
</tcenterdconf>
EOF
}

auto_tconnd_xml() {
    cat > $TCONND_HOME/cfg/tconnd.xml.auto <<EOF
<tconnd version="2147483647">
    <Threading> 0</Threading>
    <MaxFD> 50   </MaxFD>
    <EnableViewer>0</EnableViewer>
    <TDRList type="TDRList">
        <Count> 1</Count>
        <TDRs type="TDR">
            <Name>tcmprotocol</Name>
            <Path>../cfg/tcm_proto.tdr</Path>
        </TDRs>
    </TDRList>
    <PDUList type="PDUList">
        <Count> 1</Count>
        <PDUs type="PDU">
            <Name>TCMPkg </Name>
            <UpSize>65536 </UpSize>
        <DownSize>0</DownSize>
            <LenParsertype>PDULENPARSERID_BY_TDR </LenParsertype>
            <LenParser type="PDULenParser">
                <TDRParser type="PDULenTDRParser">
                                         <TDR>tcmprotocol</TDR>
                                        <Pkg>TCMPkg</Pkg>
                                        <PkgLen>Head.PkgLen</PkgLen>
                </TDRParser>
            </LenParser>
        </PDUs>
    </PDUList>
    <ListenerList type="ListenerList">
        <Count> 1</Count>
        <Listeners type="Listener">
            <Name>default</Name>
            <Url>tcp://0.0.0.0:${TCONND_LISTEN_PORT}?reuse=1</Url>
                        <SendBuff> 655360 </SendBuff>
                        <RecvBuff> 655360 </RecvBuff>
           <MaxIdle>18000</MaxIdle>
      </Listeners>
    </ListenerList>
    <SerializerList type="SerializerList">
        <Count> 2</Count>
        <Serializers type="Serializer">
            <Name>tcmcenter    </Name>
            <Url>${TCMCENTER_BUS_ID}</Url>
        </Serializers>
    </SerializerList>
        <NetTrans type="NetTrans">
            <Name>tcmmsg</Name>
            <PDU>TCMPkg</PDU>
            <Listener>default</Listener>
            <Serializer>tcmcenter    </Serializer>
        </NetTrans>
</tconnd>
EOF
}

auto_tcmconsole_xml() {
	cat > $TCMCENTER_HOME/cfg/tcmconsole.xml.auto <<EOF
<?xml version="1.0" encoding="GBK" standalone="yes" ?>

<tcmconsole __version="2">

    <!--DEFAULT VALUE: '127.0.0.1:9010'-->
    <TbusconfigcenterUrl>127.0.0.1:$TCONND_LISTEN_PORT</TbusconfigcenterUrl>

    <!--connTimeout: connect tcmcenter timeout,default 10000ms-->
    <!--DEFAULT VALUE: '10000'-->
    <connTimeout>10000 </connTimeout>

    <!--SayHellpGap: time gap for say hellp to server -->
    <!--DEFAULT VALUE: '30'-->
    <SayHellpGap>30 </SayHellpGap>

    <!--ExecuteTimeout: tcmconsole updating timeout because last cmd res not return,default 60s-->
    <!--DEFAULT VALUE: '60'-->
    <ExecuteTimeout>60 </ExecuteTimeout>
</tcmconsole>
EOF
}

auto_tcmcenter_xml() {
    sed -r -e 's/CenterdAddr="([0-9]\.){3}[0-9]"/CenterdAddr="'$TCENTER_BUS_ID'"/' \
           -e 's/TconndAddr="([0-9]\.){3}[0-9]"/TconndAddr="'$TCONND_BUS_ID'"/' \
    $TCMCENTER_HOME/cfg/tcmcenter.xml > $TCMCENTER_HOME/cfg/tcmcenter.xml.auto 
}

auto_tagent_xml() {
    	cat > $TAGENT_HOME/cfg/tagent.xml.auto <<EOF
<?xml version="1.0" encoding="GBK" standalone="yes" ?>

<tagentconf __version="1">

    <master>${ETH1_IP}:${TCENTER_LISTEN_PORT}</master>
    <slave>${ETH1_IP}:${TCENTER_LISTEN_PORT}</slave>

    <!--DEFAULT VALUE: 'lib'-->
    <lib>../lib</lib>

    <!--DEFAULT VALUE: 'cfg'-->
    <cfg>../cfg</cfg>

    <!--DEFAULT VALUE: '500000.000000'-->
    <tick>500000.000000 </tick>

</tagentconf>
EOF
}

tconnd() {
	if [[ $1 = "start" ]]; then
		echo "start tconnd...."
		cd ${TCONND_HOME}/bin/ && \
		${TCONND_HOME}/bin/tconnd --id=$TCONND_BUS_ID \
			--use-bus --bus-key=$GCIM_SHM_KEY \
			--conf-file=${TCONND_HOME}/cfg/tconnd.xml.auto \
			--tlogconf=${TCONND_HOME}/cfg/tconnd_log.xml -D start
	elif [[ $1 = "stop" ]]; then
		 ${TCONND_HOME}/bin/tconnd --id=$TCONND_BUS_ID stop
	else
		echo "Usage: $FUNCNAME [start|stop]"
		exit 1
	fi
}

tcenterd() {
	if [[ $1 = "start" ]]; then
		echo "start tcenterd...."
		cd ${TCENTER_HOME}/bin/ && \
		${TCENTER_HOME}/bin/tcenterd --id=$TCENTER_BUS_ID \
			--use-bus --bus-key=$GCIM_SHM_KEY \
			--conf-file=${TCENTER_HOME}/cfg/tcenterd.xml.auto \
			--tlogconf=${TCENTER_HOME}/cfg/tcenterd_log.xml -D start
	elif [[ $1 = "stop" ]]; then
		 ${TCENTER_HOME}/bin/tcenterd --id=$TCENTER_BUS_ID stop
	else
		echo "Usage: $FUNCNAME [start|stop]"
		exit 1
	fi
}

tcmcenter() {
	if [[ $1 = "start" ]]; then
		echo "start tcmcenter..."
		cd ${TCMCENTER_HOME}/bin/ && \
		${TCMCENTER_HOME}/bin/tcmcenter --id=${TCMCENTER_BUS_ID} --bus-key=$GCIM_SHM_KEY \
			--conf-format=3 --bus-beat-gap=600 \
			--conf-file=${TCMCENTER_HOME}/cfg/tcmcenter.xml.auto \
			--tlogconf=${TCMCENTER_HOME}/cfg/tcmcenter_log.xml -D start
	elif [[ $1 = "restart" ]]; then
		echo "restart tcmcenter..."
		cd ${TCMCENTER_HOME}/bin/ && \
		${TCMCENTER_HOME}/bin/tcmcenter --id=${TCMCENTER_BUS_ID} --bus-key=$GCIM_SHM_KEY \
			--conf-format=3 --bus-beat-gap=600 \
			--conf-file=${TCMCENTER_HOME}/cfg/tcmcenter.xml.auto \
			--tlogconf=${TCMCENTER_HOME}/cfg/tcmcenter_log.xml -D restart
	elif [[ $1 = "stop" ]]; then
		 ${TCMCENTER_HOME}/bin/tcmcenter --id=${TCMCENTER_BUS_ID} stop
	else
		echo "Usage: $FUNCNAME [start|stop|restart]"
		exit 1 
	fi
}

tagent() {
	if [[ $1 = "start" ]]; then
	    	echo "start tagent..."
	    	cd ${TAGENT_HOME}/bin/ && 
		${TAGENT_HOME}/bin/tagent --id=${TAGENT_BUS_ID} --tlogconf=${TAGENT_HOME}/cfg/tagent_log.xml \
			--conf-file=${TAGENT_HOME}/cfg/tagent.xml.auto \
			--log-file=${TAGENT_HOME}/log/tagent start -D
	elif [[ $1 = "stop" ]]; then
		${TAGENT_HOME}/bin/tagent --id=${TAGENT_BUS_ID} stop
	else 
	        echo "Usage: $FUNCNAME [start|stop]"                                
                exit 1
        fi
}

tcmconsole() {
	if [[ $# -eq 0  ]]; then
		$TCMCENTER_HOME/bin/tcmconsole --id=${TCMCONSOLE_BUS_ID} --conf-file=$TCMCENTER_HOME/cfg/tcmconsole.xml.auto \
			--tlogconf=$TCMCENTER_HOME/cfg/tcmconsole_log.xml start
	else
		$TCMCENTER_HOME/bin/tcmconsole --id=${TCMCONSOLE_BUS_ID} --conf-file=$TCMCENTER_HOME/cfg/tcmconsole.xml.auto \
			--tlogconf=$TCMCENTER_HOME/cfg/tcmconsole_log.xml \
			--cmd="$@" --timeout 900 start
	fi
}

tcm() {
	if [[ $1 = "stop" ]]; then
		for comp in tcmcenter tcenterd tconnd 
		do
			$comp stop 
		done
	elif [[ $1 = "start" ]]; then
		for comp in tcmcenter tcenterd tconnd 
		do
			$comp start 
		done
	fi
}

check_tcm_cmd_log() {
    local log=$1
    local status="false"

    status=$(awk '/total\(([0-9]+)\), Succeed\(([0-9]+)\),/ {
            match($0, /total\(([0-9]+)\), Succeed\(([0-9]+)\),/, a) 
            if(a[1] == a[2]) 
                    print "true"
            else
                    print "false"
            exit
         }
    
         /\[SUCCEED\]/ {
                 print "true"
         }' $log)
    #status=$(perl -lne '/total\(([0-9]+)\), Succeed\(([0-9]+)\),/ && ( $1 == $2 ? print "true" : print "false" )' $log)
    [[ "$status" = "true" ]]
}

cmd() {
    CMD=$@
    local cmd_log=~/.cmd.log
    local tmp_cmd_log=~/.cmd.log.tmp
    
    # if arguments are empty return false
    if [[ $# -eq 0 ]]; then
        echo "$FUNCNAME <tcm_cmd> <arguments>"
        return 1
    fi

    # run the command
    ( echo -e "\n\r\c" ; echo "start run [$CMD] @ $(date "+%Y%m%d %H:%M:%S") on $ETH1_IP"; \
    tcmconsole "$CMD") 2>/dev/null > $tmp_cmd_log

    if check_tcm_cmd_log $tmp_cmd_log; then
        cat $tmp_cmd_log | tee -a $cmd_log
        return 0
    else
        echo -e "\n\r\c"
        echo "run [$CMD] failed. check following details:"
        cat $tmp_cmd_log
        return 1
    fi
}

# check users
check_user() {
	if whoami | grep -vEq 'user0[0-9]' 2>/dev/null ; then
		echo "please use user00-user09"
		exit 1
	fi
}
		
usage() {
    echo "Usage $0 <tcm|tagent|tcmcenter|tconnd|tcenterd> <start|stop>"
    echo "      $0 tcm_init  # inital tcm bus channel & generate config"
    echo "      $0 cmd 	     # open tcmconsole"
    echo "      $0 cmd <tcm_console_commands>  # run tcm commands without a console"
    echo "      $0 tcm_init"
    echo "      $0 -h/--help"
    exit 1
}

list() {
    if [[ "$1" == "all" ]]; then
	printf "%s\n" "${TCM_COMP[@]}"
	printf "%s\n" "${TCM_ADMIN_CMDS[@]}"
    elif [[ "$1" == "cmd" ]]; then
	printf "%s\n" "${TCM_CONSOLE_CMD[@]}"
    else
	echo "wrong args."
	exit 1
    fi
}

#check_user
case $1 in
	'cmd')
		if [[ $# -eq 1 ]]; then
		    tcmconsole
		else
		    shift 1
		    cmd $@ 
	        fi
		;;
	tcm|tagent|tcmcenter|tconnd|tcenterd|tcmconsole) 
		if echo "$2" | grep -Eq 'start|stop|restart' 2>/dev/null; then
			$1 $2
		else
			echo "Usage: $1 <start|stop|restart>"
			exit 1
		fi
		;;
	tcm_init)	tcm_bus_init ;;
	list)	shift 1; list $@ ;;

	*) 	usage ;;
esac
