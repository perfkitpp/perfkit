import {theme} from "../App";
import {EmptyFunc, useForceUpdate, useInterval, useWebSocket} from "../Utils";
import {createContext, useContext, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {Col, Container, Row, Spinner} from "react-bootstrap";
import findValueStyle from "./Style";

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
    nodes: [number/*Index*/, TracerNodeValue][],
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
  f_F: boolean, // flag-FOLD
  f_S: boolean, // flag-SUBS
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

    switch (content.method) {
      case "node_new": {
        const {params} = content;
        const root = allTracers.current[params.tracer];
        root.waitTimeout = undefined;
        root.fenceNodeID = params.fence_node_id;

        // Append nodes to given root
        root.notifyNewNodeAdded(params.nodes);
        break;
      }
      case "node_update": {
        const {params} = content;
        const root = allTracers.current[params.tracer];
        root.waitTimeout = undefined;
        root.fenceUpdate = params.fence_update;
        root.notifyNodeDataUpdate(params.nodes);
        break;
      }
      case "tracer_instance_new":
        content.params.map(name => allTracers.current[name] = createTracerContext(name));
        forceUpdateAll();
        forceUpdateComponentList();
        break;
      case "tracer_instance_destroy":
        delete allTracers.current[content.params];
        forceUpdateAll();
        forceUpdateComponentList();
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
        <Col key={rootName} className='p-1 m-0' style={{minWidth: '60ch', maxWidth: '120ch'}}>
          <TraceRootNode context={allTracers.current[rootName]} key={rootName}/>
        </Col>
      )
    , [componentListUpdate]);

  if (sockTrace?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return <div className='h-100'>
    <TraceSocketContext.Provider value={sockTrace as WebSocket}>
      {/*TODO: Search text overlay here!*/}
      <hr className='m-2'/>
      <Container fluid
                 className='p-0 m-0 mh-100'
                 style={{
                   fontFamily: 'Lucida Console, monospace',
                   fontSize: '1rem',
                   overflowY: 'scroll'
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
      if (context.waitTimeout && Date.now() < context.waitTimeout) {
        // If still waiting for previous request ...
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
      <label htmlFor="customRange"/>
      <input type='range' className='form-range h-100' min='10' max='2000' id='customRange'
             value={intervalValue}
             onInput={ev => setIntervalValue(parseInt(ev.currentTarget.value))}/>
      <span className='text-nowrap ms-2 text-secondary w-25'>{intervalValue}ms</span>
    </span>;
  }

  function onNew(nodes: TracerNodeDesc[]) {
    for (const node of nodes) {
      // Build new node
      context.all[node.unique_index] = {
        props: node, body: {
          T: "P", f_F: false, f_S: false, V: null
        }, children: []
      };

      if (node.parent_index >= 0) {
        const parent = context.all[node.parent_index];
        parent.children.push(node.unique_index);
        parent.notifyChildAdded && parent.notifyChildAdded();
      } else if (context.roots.indexOf(node.unique_index) == -1) {
        context.roots.push(node.unique_index);
      }
    }

    updateNodeList();
  }

  function onUpdate(nodes: [number, TracerNodeValue][]) {
    // Re-render all updated subnodes
    for (const [index, value] of nodes) {
      try {
        const node = context.all[index];
        node.body = value;
        node.notifyUpdate && node.notifyUpdate();
      } catch {
        // NOP
      }
    }
  }

  // TODO: Control fetching load-balance here.
  const {context} = prop;
  const [enabled, setEnabled] = useState(false);
  const [nodeListUpdate, updateNodeList] = useReducer(v => v + 1, 0);

  // Perform registration
  useEffect(() => {
    context.notifyNodeDataUpdate = onUpdate;
    context.notifyNewNodeAdded = onNew;
    return () => {
      context.notifyNodeDataUpdate = EmptyFunc;
      context.notifyNewNodeAdded = EmptyFunc;
    }
  }, [])

  // Render root nodes
  const rootNodes = useMemo(() => context.roots.map(
    rootIdx => <div key={rootIdx} className='ms-2'>
      <TraceNode root={context} context={context.all[rootIdx]}/>
    </div>
  ), [nodeListUpdate]);

  return <div className='w-100 rounded-3 bg-opacity-10 bg-secondary float'>
    <div className='btn w-100 text-start d-flex flex-row align-items-center'
         onClick={() => setEnabled(v => !v)}>
      {enabled
        ? <i className='ri-base-station-line text-success me-2'/>
        : <i className='ri-zzz-line me-2 text-light'/>}
      <div className={enabled ? 'text-success' : 'text-light'}>
        {context.name}
      </div>
      <div className='flex-grow-1 my-0 mx-3 d-flex justify-content-end'>
        <hr className='text-center my-0 flex-grow-0 rounded-5'
            style={{
              transition: 'width 1s, height 0.3s, background 1s',
              width: !enabled ? '0' : '100%',
              height: !enabled ? '0' : '0.5em',
              background: !enabled ? theme.secondary : theme.success,
              opacity: '50%'
            }}/>
      </div>
      {enabled && <DataFetchControl/>}
    </div>
    {enabled && <div>
      {rootNodes}
    </div>}
  </div>;
}

function TraceNode(props: { root: TracerContext, context: TracerNodeContext }) {
  function NameAndValue() {
    function valueStyle() {
      if (context.body.T === 'T')
        return ["ri-timer-line", '#387acc'];
      else if (context.body.V === null)
        return ["ri-loader-line", theme.secondary];
      else
        return findValueStyle(context.body.V);
    }

    function toggleSubsState(ev: { stopPropagation: () => void }) {
      ev.stopPropagation();

      sockTrace.send(JSON.stringify({
        method: "node_control",
        params: {
          tracer: root.name,
          node_index: context.props.unique_index,
          subscribe: !context.body.f_S,
          fold: collapsed
        }
      } as ReqTraceControl));
    }

    function toggleFoldState(ev: { stopPropagation: () => void }) {
      setCollapsed(v => !v);
      ev.stopPropagation();
    }

    function onUpdateValue() {
      frameRef.current.style.transition = '0s';
      frameRef.current.style.background = typeColor + '22';
      setTimeout(() => {
        if (frameRef.current) {
          frameRef.current.style.transition = '0.2s';
          frameRef.current.style.background = '0';
        }
      }, 100);

      if (collapsed != context.body.f_F) {
        setCollapsed(context.body.f_F);
      }

      forceUpdate();
    }

    const forceUpdate = useForceUpdate();
    const [icon, typeColor] = valueStyle();
    const [hovering, setHovering] = useState(false);
    const frameRef = useRef(null as any as HTMLDivElement);

    useEffect(() => {
      // Register update event receiver
      context.notifyUpdate = onUpdateValue;
    }, []);

    return <div className='p-1 mx-2 d-flex align-items-center rounded-3 border-bottom border-dark'
                onMouseEnter={() => setHovering(true)} onMouseLeave={() => setHovering(false)}
                onClick={ev => toggleFoldState(ev)}>
      <i className={icon + ' me-2'} style={{color: typeColor}}/>
      <span className='me-2'
            style={{color: context.children.length > 0 && collapsed ? theme.secondary : theme.light}}>{props.context.props.name}</span>
      <div className='btn py-0 px-2'
           style={{transition: '0.2s', opacity: hovering ? '100%' : (context.body.f_S ? '80%' : '0%')}}
           onClick={ev => toggleSubsState(ev)}>
        {context.body.f_S
          ? <i className='ri-pulse-line text-success'/>
          : <i className='ri-rest-time-line text-danger'/>}
      </div>
      <div className='flex-grow-1 my-0 py-0 px-0 mx-4 d-flex justify-content-end'>
        <hr className='rounded-5 flex-grow-0'
            style={{
              transition: '0.5s',
              margin: '0',
              color: typeColor, opacity: hovering ? '50%' : '0%',
              width: hovering ? '100%' : '0%',
              height: hovering ? '0.2em' : '1em'
            }}/>
      </div>
      <span style={{color: typeColor}} ref={frameRef} className={'px-2'}>{
        context.body.T === "T"
          ? <span>{(context.body.V * 1e3).toFixed(3)} <span className='text-secondary small'> ms</span></span>
          : stringify(context.body.V)
      }</span>
    </div>;
  }

  const {root, context} = props;
  const [collapsed, setCollapsed] = useState(context.body.f_F);
  const sockTrace = useContext(TraceSocketContext);
  const [childrenListChanged, notifyChildrenListChanged] = useReducer(v => v + 1, 0);

  useEffect(() => {
    context.notifyChildAdded = notifyChildrenListChanged;
  }, []);

  useEffect(() => {
    // If collapse state changes, send control command
    sockTrace.send(JSON.stringify({
      method: "node_control",
      params: {
        tracer: root.name,
        node_index: context.props.unique_index,
        fold: collapsed
      }
    } as ReqTraceControl));
  }, [collapsed]);

  const children = useMemo(() => context.children.map(
    childIdx => <TraceNode key={childIdx} root={root} context={root.all[childIdx]}/>
  ), [childrenListChanged]);

  return <span>
    <NameAndValue/>
    <div className='ms-4'>
    {!collapsed && children}
    </div>
  </span>
}

function stringify(value: any) {
  return typeof value === "string" ? value : JSON.stringify(value);
}

interface TracerContext {
  name: string;
  all: TracerNodeContext[]; // [unique_index <-> Context]
  roots: number[];
  updateIntervalMs: number;

  notifyNewNodeAdded: (nodes: TracerNodeDesc[]) => void;
  notifyNodeDataUpdate: (nodes: [number, TracerNodeValue][]) => void;

  waitTimeout?: number; // Is waiting for fetched data already ?

  fenceUpdate: number;
  fenceNodeID: number;
}

function createTracerContext(name: string): TracerContext {
  return {
    name, all: [], roots: [],
    updateIntervalMs: 100,
    fenceUpdate: 0, fenceNodeID: 0,
    notifyNodeDataUpdate: EmptyFunc,
    notifyNewNodeAdded: EmptyFunc
  }
}

interface TracerNodeContext {
  props: TracerNodeDesc,
  body: TracerNodeValue
  children: number[];

  notifyUpdate?: () => void;
  notifyChildAdded?: () => void;
}
