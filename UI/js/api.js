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

async function getStatus() {
  const response = await fetch(
    `${ESP8266_API_URL}/api/status`,
    { cache: "no-store" }
  );

  if (!response.ok) {
    throw new Error("No se pudo consultar el estado del ESP8266");
  }

  return await response.json();
}



async function setControlMode(mode) {
  const response = await fetch(`${ESP8266_API_URL}/api/mode`, {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded"
    },
    body: new URLSearchParams({ mode })
  });

  if (!response.ok) {
    const result = await response.json().catch(() => ({}));
    throw new Error(
      result.error || "No se pudo cambiar el modo de funcionamiento"
    );
  }

  return await response.json();
}
