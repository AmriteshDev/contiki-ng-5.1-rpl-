// Set timeout for 30 minutes (1800000 milliseconds)
TIMEOUT(1800000, log.log("30 Minutes completed. Stopping simulation.\n"); log.testOK());

while (true) {
  /* Ye loop background events ko process hone deta hai.
     Jaise hi 30 min poore honge, TIMEOUT trigger hoga aur simulation stop ho jayegi. */
  YIELD();
}