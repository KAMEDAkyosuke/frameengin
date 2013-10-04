var dgram = require("dgram");
var echo = dgram.createSocket("udp4");
echo.on("error", function (err) {
    console.log("server error:\n" + err.stack);
    echo.close();
});
 
echo.on("message", function (msg, rinfo) {
    console.log("server got: " + msg + " from " + rinfo.address + ":" + rinfo.port);
    echo.send(msg, 0, msg.length, rinfo.port, rinfo.address);
});

echo.bind(12345);
