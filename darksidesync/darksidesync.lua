---------------------------------------------------------------------
-- DarkSideSync is a Lua helper module for asynchroneous callbacks from
-- other libraries. Lua is single-threaded by nature and hence working with
-- multithreaded libraries is a complex matter. DarkSideSync aim is to make
-- using asynchroneous libraries simple.
-- <br/>DarkSideSync takes away the complexity of messages queues,
-- locking, synchronization, etc. because it implements them once and has a
-- thread safe API to perform all those tasks, and notify Lua of incoming
-- threads/data. It is a regular Lua module (C library) that can be loaded
-- from Lua (no C-side dependencies/linking for any libraries using DarkSideSync)
-- and it supports many libraries to consume its services simultaneously.
-- <br/><br/><a href="../dss_overview.htm">Check here for an overview.</a>
-- <br/><br/>It can only work with libraries designed to work with DarkSideSync. Check
-- out the library source, specifically <a href="https://github.com/Tieske/DarkSideSync/blob/master/darksidesync/darksidesync_api.h"><code>darksidesync_api.h</code></a> on how
-- to do that. Additionally use <a href="https://github.com/Tieske/DarkSideSync/blob/master/darksidesync/darksidesync_aux.c"><code>darksidesync_aux.c</code></a> to get up and
-- running with DarkSideSync quickly (just an include of this file will get
-- you 95% done)
-- <br/>
-- <br/>To use the DarkSideSync library from Lua there are 2 options
-- <ul><li>do not use notifications, but regularly call <a href="#poll"><code>poll()</code></a>
-- to check for incomming data</li>
-- <li>use the UDP notification mechanism (a LuaSocket implementation is available
-- in <a href="../files/dss.html"><code>dss.lua</code></a>).</li></ul>
-- The latter has UDP networking overhead but has some advantages; works with any network library and
-- allows the application to 'go to sleep' in a network <code>select()</code> call. Additionally a UDP socket
-- has the advantage (over a filehandle) that it works on most common platforms.
-- In cases with a high number of callbacks the polling method is considered the better solution.
-- @class module
-- @name darksidesync
-- @copyright 2012 Thijs Schreijer, DarkSideSync is free software under the MIT/X11 license
-- @release Version 0.x, DarkSideSync.




Do not run this file. It is only created to document the darksidesync library
through LuaDoc (hence this non-comment line will error out.

local dummy = {

----------------------------------------------------------------------------------------
-- Lua function to get the next item from the queue.
-- If you use the UDP notifications, you <strong>MUST</strong> also read from the UDP socket to
-- clear the received packet from the socket buffer
-- @return (by DSS) queuesize of remaining items (or -1 if there was nothing on the queue to begin with)
-- @return (by client library) Lua callback function to handle the data
-- @return (by DSS) a 'waiting-thread' userdata waiting for the response (only if the async callback expects Lua to deliver a result, in this case the async callback thread will be blocked until <a href="#[waitingthread].setresults"><code>[waitingthread].setresults([return arguments])</code></a> is called)
-- @return (by client library) any other parameters as delivered by the async callback.
-- @usage# local runcallbacks()
--   local results = { darksidesync.poll() }
--   local count = results[1]
--   table.remove(results, 1)
--   local callback = results[1]
--   table.remove(results, 1)
--   if count == -1 then
--     return
--   end
--   if callback then
--     callback(unpack(results))
--   end
--   if count > 0 then
--     print("there is more to do; " .. tostring(count) .. " items are still in the queue.")
--   else
--     print("We're done for now.")
--   end
-- end

function poll()
end,
----------------------------------------------------------------------------------------
-- Lua function to get the UDP port currently in use for notifications, or 0 if
-- UDP notifications are currently disabled.
-- @return UDP port number
-- @see setport
function getport()
end,
----------------------------------------------------------------------------------------
-- Lua function to set the UDP port for notifications. The IP address the notification
-- will be send to will always be <code>localhost</code> (loopback adapter).
-- @param port UDP port number to use for notification packets. A value from 0 to 65535,
-- where 0 will disable notifications.
-- @return 1 if successfull, or <code>nil + error msg</code> if it failed
-- @see getport
function setport(port)
end,
----------------------------------------------------------------------------------------
-- Lua function to set the results of an async callback. The 'waiting-thread' userdata is collected from
-- the <a href="#poll"><code>poll()</code></a> method in case a thread is blocked and waiting for a result.
-- Call this function with the results to return to the async callback.
-- @name [waitingthread].setresults
-- @param ... parameters to be delivered to the async callback. This depends on what the client library expects
-- @return depends on client library implementation
-- @see poll
function setresults(...)
end,
}
