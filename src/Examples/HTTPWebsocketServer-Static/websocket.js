'use strict';

let websocket = null;
let queued_messages = [];
let state_str = 'disconnected';

let on_websocket_connect = null;
let on_websocket_disconnect = null;
let on_websocket_message = null;

export function state() {
  return state_str;
}

export function connect(on_websocket_connect, on_websocket_disconnect, on_websocket_message) {
  if (state_str !== 'disconnected') {
    throw new Error('attempted to connect websocket when not disconnected');
  }

  state_str = 'connecting';
  const ws_proto = document.location.protocol.startsWith('https') ? 'wss' : 'ws';
  websocket = new WebSocket(`${ws_proto}://${window.location.host}/stream`);

  websocket.onopen = function (event) {
    state_str = 'connected';
    if (on_websocket_connect !== null) {
      on_websocket_connect();
    }
    for (const message of queued_messages) {
      websocket.send(message);
    }
    queued_messages = [];
  };

  websocket.onmessage = function (event) {
    if (on_websocket_message !== null) {
      on_websocket_message(event.data);
    }
  };

  websocket.onclose = function (event) {
    state_str = 'disconnected';
    if (on_websocket_disconnect !== null) {
      on_websocket_disconnect();
    }
  };
}

export function send_message(message) {
  if (websocket === null) {
    throw new Error("websocket is not open");
  }

  if (websocket.readyState === 0) { // CONNECTING
    queued_messages.push(message);
  } else if (websocket.readyState === 1) {
    websocket.send(message);
  } else {
    throw new Error("websocket is closed");
  }
}
