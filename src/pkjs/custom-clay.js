module.exports = function(minified) {
  var clayConfig = this;
  var slots = ['ComplicationLeft', 'ComplicationMiddle', 'ComplicationRight'];

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    slots.forEach(function(key) {
      clayConfig.getItemByMessageKey(key).on('change', function() {
        var selected = this.get();
        if (selected === '0') return;

        slots.forEach(function(otherKey) {
          if (otherKey === key) return;
          var otherItem = clayConfig.getItemByMessageKey(otherKey);
          if (otherItem.get() === selected) {
            otherItem.set('0');
          }
        });
      });
    });
  });
};
