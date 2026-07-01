package chipyard

import org.chipsalliance.cde.config.{Config}

class LFSRDemoRocketConfig extends Config(
  new chipyard.example.WithLFSR ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
