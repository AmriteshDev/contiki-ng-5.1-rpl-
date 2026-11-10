// Set timeout for 30 minutes (1800000 milliseconds)
TIMEOUT(1800000, log.log("30 Minutes completed. Stopping simulation.\n"); log.testOK());

while (true)
{
  // Yeh line headless mode mein node ke output ko log file mein likhti hai
  if (msg)
  {
    log.log(time + "\tID:" + id + "\t" + msg + "\n");
  }
  YIELD();
}