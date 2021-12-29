'use strict';

function create_element(type, options, children) {
  const element = document.createElement(type);
  for (const [key, value] of Object.entries(options)) {
    // this typecheck is needed for setting e.g. onclick, which should be a code
    // object instead of a string/number (so setAttribute doesn't work). we also
    // allow null and undefined to make conditionally setting attributes easier
    if (value === null || value === undefined) {
      continue;
    }
    if (typeof value === 'string' || typeof value === 'number') {
      element.setAttribute(key, value);
    } else {
      element[key] = value;
    }
  }

  if (typeof children === 'string') {
    element.innerHTML = children;
  } else {
    if (!Array.isArray(children)) {
      children = [children];
    }
    for (const child of children) {
      if (typeof child === 'string') {
        element.appendChild(text(child));
      } else if (child === null || child === undefined) {
        continue;
      } else {
        element.appendChild(child);
      }
    }
  }
  return element;
}



export function a(options = {}, children = []) {
  return create_element('a', options, children);
}

export function b(children = []) {
  return create_element('b', {}, children);
}

export function i(children = []) {
  return create_element('i', {}, children);
}

export function u(children = []) {
  return create_element('u', {}, children);
}

export function div(options = {}, children = []) {
  return create_element('div', options, children);
}

export function span(options = {}, children = []) {
  return create_element('span', options, children);
}

export function br(options = {}) {
  return create_element('br', options, []);
}

export function table(options = {}, children = []) {
  return create_element('table', options, children);
}

export function th(options = {}, children = []) {
  return create_element('th', options, children);
}

export function tr(options = {}, children = []) {
  return create_element('tr', options, children);
}

export function td(options = {}, children = []) {
  return create_element('td', options, children);
}

export function pre(children = []) {
  return create_element('pre', {}, children);
}

export function text(text) {
  return document.createTextNode(text);
}

export function input(options = {}, children = []) {
  return create_element('input', options, children);
}

export function textarea(options = {}, children = []) {
  return create_element('textarea', options, children);
}

export function button(options = {}, children = []) {
  return create_element('button', options, children);
}

export function img(options = {}) {
  return create_element('img', options, []);
}

export function label(options = {}, children = []) {
  return create_element('label', options, children);
}

export function select(options = {}, children = []) {
  return create_element('select', options, children);
}

export function option(options = {}, children = []) {
  return create_element('option', options, children);
}



export function clear() {
  document.body.textContent = '';
}

export function set_background_color(color) {
  document.body.style.backgroundColor = color;
}

export function add(root) {
  if (Array.isArray(root)) {
    for (const element of root) {
      document.body.appendChild(element);
    }
  } else {
    document.body.appendChild(root);
  }
}

export function remove_from_root(element) {
  document.body.removeChild(element);
}

export function width() {
  return document.body.innerWidth;
}

export function height() {
  return document.body.innerHeight;
}
