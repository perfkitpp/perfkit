import {theme} from "../App";
import {useForceUpdate, useWebSocket} from "../Utils";
import {useRef} from "react";

interface MsgTracerInstanceNew {
  method: 'tracer_instance_new'
  params: string[]
}

interface MsgTraceNodeNew {
  method: 'node_new'
  params: {
    tracer: string,
    nodes: TracerNodeDesc[];
  }
}

interface MsgTraceNodeUpdate {
  method: 'node_update'
  params: {
    tracer: string,
    nodes: [number, TracerNodeValueBase][],
  }
}

interface MsgTracerInstanceDestroy {
  method: 'tracer_instance_destroy'
  params: string
}

interface ReqTracerFetch {
  method: 'node_fetch',
  params: {
    tracer: string,
    fence_update: number,
    fence_node_id: number,
  }
}

interface ReqTraceControl {
  method: 'node_control',
  params: {
    tracer: string,
    node_index: number,
    fold?: boolean,
    subscribe?: boolean,
  }
}


interface TracerNodeDesc {
  unique_index: number,
  name: string,
  path: string[],
  // TODO: What do I need ?
}

interface TracerNodeValueBase {
  "f-F": boolean, // flag-FOLD
  "f-S": boolean, // flag-SUBS
}

interface TracerNodeValue_P extends TracerNodeValueBase {
  T: "P";
  V: string | boolean | number | null
}

interface TracerNodeValue_T extends TracerNodeValueBase {
  T: "T";
  V: number
}

type TracerNodeValue = TracerNodeValue_P | TracerNodeValue_T;

export default function TracePanel(prop: { socketUrl: string }) {
  function onRecvTraceMsg(ev: MessageEvent) {
    const content = JSON.parse(ev.data) as MsgTraceNodeNew | MsgTraceNodeUpdate | MsgTracerInstanceNew | MsgTracerInstanceDestroy;

    switch (content.method) {
      case "node_new":
        break;
      case "node_update":
        break;
      case "tracer_instance_new":
        content.params.map(name => allTracers.current[name] = createTracerContext(name));
        forceUpdateAll();
        break;
      case "tracer_instance_destroy":
        break;
    }
  }

  function onCloseConnection() {
    // TODO: Reset all state
  }

  const allTracers = useRef({} as { [key: string]: TracerContext })
  const sockTrace = useWebSocket(prop.socketUrl, {onmessage: onRecvTraceMsg, onclose: onCloseConnection}, []);
  const forceUpdateAll = useForceUpdate();

  return <div>
    <div>
      <span>Search Panel</span>
    </div>
  </div>;
}

function TraceRootHeader(prop: {
  name: string
}) {

}

interface TracerContext {
  name: string;
  all: TracerNodeContext[]; // [unique_index <-> Context]
  roots: number[];
}

function createTracerContext(name: string): TracerContext {
  return {name, all: [], roots: []}
}

interface TracerNodeContext {
  props: TracerNodeDesc,
  body: TracerNodeValue
}
