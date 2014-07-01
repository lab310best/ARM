#
# Makefile for the WatchDog device drivers.
#

obj-$(CONFIG_MCP) += mcp.o

mcp-objs := mcpcan.o spi.o spi_cmd.o


# Only one watchdog can succeed. We probe the hardware watchdog
# drivers first, then the softdog driver.  This means if your hardware
# watchdog dies or is 'borrowed' for some reason the software watchdog
# still gives you some cover.


