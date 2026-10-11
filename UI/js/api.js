const ESP8266_API_URL = "http://192.168.1.34";

async function controlRelay(state) {
  const response = await fetch(
    `${ESP8266_API_URL}/api/control`,
    {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded"
      },
      body: new URLSearchParams({ state })
    }
  );

  const result = await response.text();

  if (!response.ok) {
    throw new Error(result);
  }

  return result;
}