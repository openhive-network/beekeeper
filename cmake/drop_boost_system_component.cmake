# boost_system is header-only since Boost 1.69 and Boost 1.89 no longer builds its
# stub library, so FindBoost fails when it is requested as a component. The plugins
# submodule still requests it; this file runs right after fc's project() call, before
# fc looks Boost up with the BOOST_COMPONENTS it inherits from plugins.
list(REMOVE_ITEM BOOST_COMPONENTS system)
