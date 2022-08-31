import {useState, useEffect, useRef, DependencyList} from 'react';
import {AppendTextToTerminal} from "./comp/Terminal";

export function useInterval(callback: any, delay: number) {
  const savedCallback = useRef(null as any); // 최근에 들어온 callback을 저장할 ref를 하나 만든다.

  useEffect(() => {
    savedCallback.current = callback; // callback이 바뀔 때마다 ref를 업데이트 해준다.
  }, [callback]);

  useEffect(() => {
    function tick() {
      savedCallback.current(); // tick이 실행되면 callback 함수를 실행시킨다.
    }

    if (delay !== null) { // 만약 delay가 null이 아니라면
      let id = setInterval(tick, delay); // delay에 맞추어 interval을 새로 실행시킨다.
      return () => clearInterval(id); // unmount될 때 clearInterval을 해준다.
    }
  }, [delay]); // delay가 바뀔 때마다 새로 실행된다.
}

//create your forceUpdate hook
export function useForceUpdate() {
  const [, setValue] = useState(0); // integer state
  return () => setValue(value => value + 1); // update state to force render
  // An function that increment 👆🏻 the previous state like here
  // is better than directly setting `value + 1`
}

// Generate unique ID
let idGenVar = 0;

export function GenerateUniqueID(prefix: string) {
  return `${prefix}${++idGenVar}`;
}

export function useWebSocket(url: string, handler: {
  onerror?: (ev: Event) => void,
  onmessage?: (ev: MessageEvent) => void,
  onclose?: (ev: CloseEvent) => void,
  onopen?: (ev: Event) => void,
}, deps: DependencyList[]) {
  const [sock, setSock] = useState(null as WebSocket | null);

  useEffect(() => {
    if (sock != null) {
      return;
    }

    const newSock = new WebSocket(url);
    setSock(prevSock => {
      prevSock?.close();
      return newSock
    });

    newSock.onclose = ev => {
      AppendTextToTerminal(
        '* ' + newSock.url + ') Disconnected from server: ' + ev.reason,
        'bg-warning bg-opacity-25');
      handler.onclose && handler.onclose(ev);
      setSock(null);
    };

    newSock.onopen = ev => {
      AppendTextToTerminal(
        '* ' + newSock.url + ') Connection established.',
        'bg-success bg-opacity-25');
      handler.onopen && handler.onopen(ev);
    }

    newSock.onerror = ev => {
      AppendTextToTerminal(
        '* ' + newSock.url + ') Error!',
        'bg-danger bg-opacity-25');
      handler.onerror && handler.onerror(ev);
    };

    newSock.onmessage = handler.onmessage ?? null;
  }, [url, sock, ...deps]);

  return sock;
}

export function useTimeout(callback: () => void, ms: number, deps: DependencyList[]) {
  const savedCallback = useRef(null as any);

  useEffect(() => {
    savedCallback.current = callback;
  }, [callback]);

  useEffect(() => {
    function onTimeout() {
      savedCallback.current();
    }

    if (ms !== null) {
      const id = setTimeout(onTimeout, ms);
      return () => clearTimeout(id);
    }
  }, [ms, ...deps]);
}

export function EmptyFunc() {
}

