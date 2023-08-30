Import("env")

env.Replace(PROGNAME="fw_esp-SolarCell_v%s" % env.GetProjectOption("custom_prog_version"))