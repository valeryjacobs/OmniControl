var HarmonyHubClient = require('../index')

HarmonyHubClient('192.168.178.25')
  .then(function (harmonyClient) {
      console.log(harmonyClient);
    return harmonyClient.getAvailableCommands()
      .then(function (commands) {
          console.log(commands);
        // Look for the first device and pick its "power" control group, pick
        // there the "poweron" function and trigger it:
        var device = commands.device[0]
        var powerControls = device.controlGroup
            .filter(function (group) { return group.name.toLowerCase() === 'power' })
            .pop()
        var powerOnFunction = powerControls['function']
            .filter(function (action) { return action.name.toLowerCase() === 'poweron' })
            .pop()

        if (powerOnFunction) {
          var encodedAction = powerOnFunction.action.replace(/\:/g, '::')
          return harmonyClient.send('holdAction', 'action=' + encodedAction + ':status=press')
        } else {
          throw new Error('could not find poweron function of first device :(')
        }
      })
      .finally(function () {
        harmonyClient.end()
      })
  })
  .catch(function (e) {
    console.log(e)
  })