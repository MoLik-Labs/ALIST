# Experimental Go cloud preview

## Scope and safety

Expose existing Go preview operations through explicit, fail-closed environment
variables so a single image can be configured without shell scripts on Docker,
Render, Railway and Heroku. This is not a complete Kova replacement. `/ping`
remains liveness only. Root storage and bearer token remain required for file
access. Writable local storage on Heroku is ephemeral and unsuitable for data.

## Tasks

1. Test and implement strict booleans for the four optional operations; CLI
   flags override environment defaults and invalid values abort startup. Existing
   defaults remain off and missing root still fails closed.
2. Add a separately named Railway config and Heroku container manifest; retain
   existing native deployment files. Document `PORT`, tokens, persistent volumes,
   non-root permission checks, limitations, and rollback.
3. Add automated checks for configuration, run Go race tests, vet, build, and
   relevant UI/docs tests. Record any platform, Docker, browser and volume tests
   that cannot run here.
