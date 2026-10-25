const ESP8266_API_URL = "http://192.168.1.39";

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
    {
      cache: "no-store",
      // Si el ESP8266 no responde en 4 s, se corta y cuenta como fallo.
      // Es menor que el intervalo de polling (5 s): las consultas no se apilan.
      signal: AbortSignal.timeout(4000)
    }
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



async function saveSchedules(schedules) {
  const body = new URLSearchParams();
  body.set("count", String(schedules.length));

  schedules.forEach((schedule, index) => {
    body.set(`start${index}`, schedule.start);
    body.set(`end${index}`, schedule.end);
  });

  const response = await fetch(`${ESP8266_API_URL}/api/schedules`, {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded"
    },
    body
  });

  const result = await response.json().catch(() => ({}));

  if (!response.ok) {
    throw new Error(result.error || "No se pudieron guardar los horarios");
  }

  return result;
}