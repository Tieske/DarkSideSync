DarkSideSync
============

Usecase
=======
I wanted to create a lua binding for a library. The library uses callbacks to report on its async operations. The main issue however was that the library created its own threadpool and the callbacks could be executed on any one of those. And that, sooner or later, will wreak havoc with Lua's single threaded nature.
So while Lua's main thread is doing its Lua thing, it needs to be notified that there is some other thread that tries to deliver some result it needs to handle. In my case the Lua thread would spend most of its time in a select() statement waiting for network IO.

Main principle
==============
Whenever a background thread has some data, it will be queued and a UDP packet will be fired on a designated port. This will wake up the select() (if the Lua thread is blocked there) and allows the main Lua thread to go and collect the returned data.

Negative
========
UDP packets require quite some overhead. So if utmost performance is required, this might not be the best solution

Positive
========
It is very generic. Its not bound to luasockets, any other network library supporting UDP will do. Cross platform, it supports Unix and Windows.

Linking
=======
The DSS library is intended for shared use and hence uses/requires a global identifier on the Lua registry. So you should not link statically unless you know what you're doing!

License
=======
The same as Lua 5.1; MIT license.

The name
========
Lua = moon, dark side of the moon, external managed threads that Lua cannot reach, you get it...