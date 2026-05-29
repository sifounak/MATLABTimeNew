module.exports = function(minified) {
  var clayConfig = this;
  var slots = ['ComplicationLeft', 'ComplicationMiddle', 'ComplicationRight'];

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var hourFormat = clayConfig.getItemByMessageKey('HourFormat');
    var leadingZeros = clayConfig.getItemByMessageKey('LeadingZeros');
    var rotateSideText = clayConfig.getItemByMessageKey('RotateSideText');

    if (!clayConfig.meta.activeWatchInfo ||
        clayConfig.meta.activeWatchInfo.platform !== 'gabbro') {
      rotateSideText.disable();
    }

    hourFormat.on('change', function() {
      if (!this.get()) {
        return;
      }

      if (leadingZeros.get() === 'none') {
        leadingZeros.set('time');
      } else if (leadingZeros.get() === 'date') {
        leadingZeros.set('dateTime');
      }
    });

    slots.forEach(function(key) {
      clayConfig.getItemByMessageKey(key).on('change', function() {
        var selected = this.get();
        if (selected === 'empty') return;

        slots.forEach(function(otherKey) {
          if (otherKey === key) return;
          var otherItem = clayConfig.getItemByMessageKey(otherKey);
          if (otherItem.get() === selected) {
            otherItem.set('empty');
          }
        });
      });
    });
  });
};
