require('luasyslog')
require('jobengine')
require('cdcsutil')

conf = {
	debug = false,
	jobDir = '../jobs.d',
	initEvent = 'sys.init',
	initArgs = {}
}

luasyslog.open('rdbinit', 'daemon')
luasyslog.log('info', 'Starting.')

engine = jobengine.new(conf)
--engine:addJob(rpcJob)
engine:init()

while engine.running do
	engine:poll()
	cdcsutil.sleep(0.1)
end

lussyslog.log('info', 'Finished.')
luasyslog.close()
