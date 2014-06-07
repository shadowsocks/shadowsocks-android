resolvers += Resolver.url("scalasbt releases", new URL("http://scalasbt.artifactoryonline.com/scalasbt/sbt-plugin-snapshots"))(Resolver.ivyStylePatterns)

resolvers += Resolver.url("madeye private releases", new URL("http://madeye-maven-repository.googlecode.com/git/ivy"))(Resolver.ivyStylePatterns)

addSbtPlugin("com.hanhuy.sbt" % "android-sdk-plugin" % "1.2.16")

addSbtPlugin("com.github.mpeltonen" % "sbt-idea" % "1.6.0")
