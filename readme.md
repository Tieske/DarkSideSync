DarkSideSync
============

Usecase
-------
I wanted to create a lua binding for a library. The library uses callbacks to report on its async operations. The main issue however was that the library created its own threadpool and the callbacks could be executed on any one of those. And that, sooner or later, will wreak havoc with Lua's single threaded nature.

So while Lua's main thread is doing its Lua thing, it needs to be notified that there is some other thread that tries to deliver some result it needs to handle. 

To support this DarkSideSync (DSS) was created, DSS has no use on its own, it will only support other libraries in handling async callbacks.

Main principle
--------------
Whenever a background thread has some data to deliver it will call a DSS method to temporarily store it in a queue. Whenever Lua has time to poll for data from this queue, the background library will get a direct call from DSS to deliver its data to Lua.

It supports async callbacks that just deliver data (the callback thread does not get blocked) and async callbacks that also expect a result (in this case the thread gets blocked until the Lua side thread has delivered the results)

There is an optional notification using a UDP packet, which is an easy way to wake up Lua from a `select()` network method.

Reference
---------
Documentation can be found in the [repository and on the GitHub page](http://tieske.github.com/DarkSideSync), a [general description and some diagrams](http://tieske.github.com/DarkSideSync/background_worker_lib_spec_0v4.pdf) are also available.

Positive
--------

- It is very generic and cross platform. Because DSS takes care of threads, locks and (optionally) sockets, a library binding for an async library may require no platform specific code and still work cross-platform
- It is setup as a separate library, loaded from Lua, no C links. It, sort of, dynamically extends the Lua C api
- Supports async callbacks that only deliver data (the callback thread is not blocked)
- Supports async callbacks that need a response (the callback thread is blocked until the Lua side response has been delivered)
- Supports multiple async background libraries simultaneously
- Supports multiple concurrent Lua states
- It has been setup with the intend to support multiple versions of the DSS API, so in the future multiple background libraries can use a single DSS library, while talking to different versions of the API
- The notification is also platform independent and even network library independent (eg. not bound to LuaSocket) any network library supporting UDP will do (this is the reason for not using file descriptors or pipes)

Negative
--------

- Notification using UDP packets requires some overhead, so for a very high number of callbacks it might be better to only use polling


Copyright & License
-------------------
Copyright 2012-2013, Thijs Schreijer.

License is the same as Lua 5.1; [MIT license](http://opensource.org/licenses/MIT).

The name
--------
Lua = moon, dark side of the moon, externally managed threads that Lua cannot reach, you get it...

Changes
-------

1.0  13-may-2013

- Initial released version