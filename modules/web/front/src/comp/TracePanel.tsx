import {theme} from "../App";
import {EmptyFunc, useForceUpdate, useInterval, useWebSocket} from "../Utils";
import {createContext, useContext, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {Col, Container, Row, Spinner} from "react-bootstrap";

interface MsgTracerInstanceNew {
  method: 'tracer_instance_new'
  params: string[]
}

interface MsgTraceNodeNew {
  method: 'node_new'
  params: {
    tracer: string,
    fence_node_id: number,
    nodes: TracerNodeDesc[];
  }
}

interface MsgTraceNodeUpdate {
  method: 'node_update'
  params: {
    tracer: string,
    fence_update: number,
    nodes: [number/*Index*/, TracerNodeValueBase][],
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
  parent_index: number,
  unique_index: number,
  name: string,
  path: string[],
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
const TraceSocketContext = createContext(null as any as WebSocket);

export default function TracePanel(prop: { socketUrl: string }) {
  function onRecvTraceMsg(ev: MessageEvent) {
    const content = JSON.parse(ev.data) as MsgTraceNodeNew | MsgTraceNodeUpdate | MsgTracerInstanceNew | MsgTracerInstanceDestroy;
    console.log(content);

    switch (content.method) {
      case "node_new":
        break;
      case "node_update":
        break;
      case "tracer_instance_new":
        content.params.map(name => allTracers.current[name] = createTracerContext(name));
        forceUpdateAll();
        forceUpdateComponentList();
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
  const [componentListUpdate, forceUpdateComponentList] = useReducer(v => v + 1, 0);

  const components = useMemo(() => Object.keys(allTracers.current)
      .map(rootName =>
        <Col className='p-1 m-0' style={{minWidth: '60ch', maxWidth: '120ch'}}>
          <TraceRootNode context={allTracers.current[rootName]} key={rootName}/>
        </Col>
      )
    , [componentListUpdate]);

  if (sockTrace?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return <div>
    <TraceSocketContext.Provider value={sockTrace as WebSocket}>
      <div className='d-flex flex-row my-2'>
        <span>Search Panel</span>
      </div>
      <hr className='m-2'/>
      <Container fluid
                 className='p-0 m-0'
                 style={{
                   fontFamily: 'Lucida Console, monospace',
                   fontSize: '1rem'
                 }}>
        <Row className='p-0 m-0'>
          {components}
        </Row>
      </Container>
    </TraceSocketContext.Provider>
  </div>;
}

function TraceRootNode(prop: {
  context: TracerContext,
}) {
  function DataFetchControl() {
    const [intervalValue, setIntervalValue] = useState(context.updateIntervalMs);
    const sockTrace = useContext(TraceSocketContext);

    useEffect(() => {
      context.updateIntervalMs = intervalValue
    }, [intervalValue]);

    useInterval(() => {
      // TODO: Send 'sync' message
      if (context.waitTimeout && Date.now() < context.waitTimeout) {
        return;
      }

      // Timeout is 10 seconds
      const msg: ReqTracerFetch = {
        method: "node_fetch",
        params: {
          tracer: context.name,
          fence_node_id: context.fenceNodeID,
          fence_update: context.fenceUpdate
        }
      };
      sockTrace.send(JSON.stringify(msg));
      context.waitTimeout = Date.now() + 10000;
    }, intervalValue);

    return <span className='w-50 d-flex align-items-center' title='Set update interval'
                 onClick={ev => ev.stopPropagation()}>
      <span className='text-nowrap me-2 text-secondary w-25'>{intervalValue}ms</span>
      <label htmlFor="customRange"/>
      <input type='range' className='form-range h-100' min='10' max='2000' id='customRange'
             value={intervalValue}
             onInput={ev => setIntervalValue(parseInt(ev.currentTarget.value))}/>
    </span>;
  }

  function OnNotifyDataArrive() {
    // TODO: Re-render all subnodes
  }

  // TODO: Control fetching load-balance here.
  const {context} = prop;
  const [enabled, setEnabled] = useState(false);

  // Perform registration
  useEffect(() => {
    context.notifyTraceDataArrival = OnNotifyDataArrive;
    return () => {
      context.notifyTraceDataArrival = EmptyFunc;
    }
  }, [])

  return <div className='w-100 rounded-3 bg-opacity-10 bg-secondary'>
    <div className='btn w-100 text-start d-flex flex-row align-items-center'
         onClick={() => setEnabled(v => !v)}>
      {enabled
        ? <i className='ri-base-station-line text-success me-2'/>
        : <i className='ri-zzz-line me-2 text-light'/>}
      <div className={enabled ? 'text-success' : 'text-light'}>
        {context.name}
      </div>
      <hr className='flex-grow-1 my-0 mx-3'/>
      {enabled && <DataFetchControl/>}
    </div>
    {enabled && <div>
        TODO: Render subnodes
    </div>}
  </div>;
}

interface TracerContext {
  name: string;
  all: TracerNodeContext[]; // [unique_index <-> Context]
  roots: number[];
  updateIntervalMs: number;

  waitTimeout?: number; // Is waiting for fetched data already ?
  notifyTraceDataArrival: () => void;

  fenceUpdate: number;
  fenceNodeID: number;
}

function createTracerContext(name: string): TracerContext {
  return {
    name, all: [], roots: [],
    updateIntervalMs: 100,
    fenceUpdate: 0, fenceNodeID: 0,
    notifyTraceDataArrival: EmptyFunc
  }
}

interface TracerNodeContext {
  props: TracerNodeDesc,
  body: TracerNodeValue
}
