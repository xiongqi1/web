set_single('service.failover.mode='+form['failoverType'],'-p');
set_single('service.failover.destaddress='+form['pingdst'],'-p');
set_single('service.failover.destaddress2='+form['pingdst2'],'-p');
set_single('service.failover.periodicpingtimer='+form['pingtimer'],'-p');
set_single('service.failover.pingacceleratedtimer='+form['pingacctmr'],'-p');
set_single('service.failover.failcount='+form['pingfailcnt'],'-p');
set_single('service.failover.succcount='+form['pingsucccnt'],'-p');
set_single('service.failover.alwaysConnectWWAN='+form['alwaysConnect'],'-p');
