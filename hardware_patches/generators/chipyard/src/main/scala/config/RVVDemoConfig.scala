package chipyard

import org.chipsalliance.cde.config.{Config}

class RVVDemoRocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(vLen = 128, dLen = 64) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
