var fs = require('fs');
var mqtt = require('mqtt');
var sleep = require('sleep');
var obj;

var currentActionIndex = 0;

fs.readFile('OmniControlSetup.json', 'utf8', function (err, data) {
  if (err) throw err;
  console.log(data);
  obj = JSON.parse(data);

  obj.Actions.forEach(function (element) {
    console.log(element.Name);
  }, this);


  var client = mqtt.connect('mqtt://test.mosquitto.org')

  client.on('connect', function () {
    client.subscribe('ORClientOut')
    client.publish('ORClientIn', 'I will.')
  })

  client.on('message', function (topic, message) {
    // message is Buffer 
    console.log(message.toString())
    if (topic = 'ORClientOut') {





      client.publish('ORClientIn', 'done');
    }
  })

});