package chipyard

import org.chipsalliance.cde.config.{Config}

class DMADemoRocketConfig extends Config(
  new chipyard.example.WithLFSRDMA ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
