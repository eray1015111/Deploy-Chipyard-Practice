package chipyard

import org.chipsalliance.cde.config.{Config}

class RoCCDemoRocketConfig extends Config(
  new chipyard.example.WithLFSRRoCC ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
