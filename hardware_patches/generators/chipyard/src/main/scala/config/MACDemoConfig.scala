package chipyard

import org.chipsalliance.cde.config.{Config}

class MACDemoRocketConfig extends Config(
  new chipyard.example.WithMAC(useBlackBox = true) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
