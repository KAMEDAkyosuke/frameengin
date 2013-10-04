var net = require('net');
var echo = net.createServer(function(c){
    console.log('echo connected');
    c.on('data', function(data) {
        console.log('echo on data');
        console.log(data);
        c.write(data);
    });
 
    c.on('end', function() {
        console.log('echo disconnected');
    });
});

echo.listen(12345, function(){
    console.log('echo start');
});
