package = "darksidesync"
version = "1.0-1"
source = {
    url = "https://github.com/Tieske/DarkSideSync/archive/version_1.0.tar.gz",
    dir = "DarkSideSync-version_1.0",
}
description = {
   summary = "Thread synchronization support for bindings to libraries with their own threadpools",
   detailed = [[
      DarkSideSync is a binding support library that makes it easy to create
      bindings to libraries that run their own background threads, like pupnp
      or OpenZwave for example. No global locks are required and no foreign
      threads will be entering the Lua environment. Bindings using DarkSideSync
      will not require platform specific code for synchronization.
   ]],
   homepage = "https://github.com/Tieske/DarkSideSync",
   license = "MIT"
}
dependencies = {
   "lua >= 5.1, < 5.2"
}
build = {
  type = "builtin",
  platforms = {
    unix = {
      modules = {
        ["darksidesync"] = {
          sources = {
            "darksidesync/darksidesync.c",
            "darksidesync/delivery.c",
            "darksidesync/locking.c",
            "darksidesync/udpsocket.c",
            "darksidesync/waithandle.c",
          },
          libraries = {
            "pthread"
          },
          defines = {
            --"LUASOCKET_DEBUG"
          }
        }
      }
    },
    win32 = {
      modules = {
        ["darksidesync"] = {
          sources = {
            "darksidesync/darksidesync.c",
            "darksidesync/delivery.c",
            "darksidesync/locking.c",
            "darksidesync/udpsocket.c",
            "darksidesync/waithandle.c",
          },
          libraries = {
            "wsock32"
          },
          defines = {
            --"LUASOCKET_API=__declspec(dllexport)"
          }
        }
      }
    }
  },
  modules = {
    ["dss"] = "darksidesync/dss.lua",
  },
--  copy_directories = { "doc", "samples", "etc", "test" }
}
