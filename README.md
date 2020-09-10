---
title: Home
---

# KatanaUsdPlugins

The KatanaUsdPlugins were originally authored by Pixar and this repository was
originally based on the [v19.11] version of the USD library.

This section contains libraries and plug-ins for Katana. We have provided a new
build mechanism to modularize the building process, allowing the plug-ins to be
built against multiple compatible USD and Katana versions (depending on API
compatibility). There is a document attached to this repository
([BUILDING.md][Building]) which describes how to go about making
your own CMake build scripts.

See the [online documentation](http://openusd.org/docs/Katana-USD-Plugins.html)
for more details.


## Pages

- [Building]
- [Change Log](CHANGELOG.md)
- [License](LICENSE.txt)
- [Notice](NOTICE.txt)


<button class="btn js-toggle-dark-mode">Dark color scheme</button>

<script>
const toggleDarkMode = document.querySelector('.js-toggle-dark-mode');

jtd.addEvent(toggleDarkMode, 'click', function(){
  if (jtd.getTheme() === 'dark') {
    jtd.setTheme('light');
    toggleDarkMode.textContent = 'Dark color scheme';
  } else {
    jtd.setTheme('dark');
    toggleDarkMode.textContent = 'Light color scheme';
  }
});
</script>

[Building]: BUILDING.md
[v19.11]: https://github.com/PixarAnimationStudios/USD/releases/tag/v19.11