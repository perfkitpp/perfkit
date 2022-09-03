import {theme} from "../App";
import {useWebSocket} from "../Utils";

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
    nodes: [number, TracerNodeValue][],
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

}

interface TracerNodeValue {
  T: "P" | "T",
  V: any,
  "f-F": boolean, // flag-FOLD
  "f-S": boolean, // flag-SUBS
}


export default function TracePanel(prop: { socketUrl: string }) {
  const sockTrace = useWebSocket(prop.socketUrl, {}, []);
  return <div>Trace Panel</div>;
}