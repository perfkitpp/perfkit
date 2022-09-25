import {ReactElement, ReactNode, StrictMode, useEffect, useMemo, useRef, useState} from 'react';
import './App.scss';
import {Nav, Navbar} from "react-bootstrap";
import {DockLayout, LayoutBase, LayoutData, PanelData, TabData} from "rc-dock";
import Terminal from "./comp/Terminal";
import ConfigPanel from "./comp/ConfigPanel";
import TracePanel from "./comp/TracePanel";
import GraphicPanel from "./comp/GraphicPanel";

export const socketUrlPrefix = process.env.NODE_ENV === "development"
  ? `ws://${window.location.hostname}:15572`
  : "ws://" + window.location.host

const curStyle = getComputedStyle(document.body);
export const theme = {
  primary: curStyle.getPropertyValue('--bs-primary'),
  secondary: curStyle.getPropertyValue('--bs-secondary'),
  success: curStyle.getPropertyValue('--bs-success'),
  info: curStyle.getPropertyValue('--bs-info'),
  warning: curStyle.getPropertyValue('--bs-warning'),
  danger: curStyle.getPropertyValue('--bs-danger'),
  light: curStyle.getPropertyValue('--bs-light'),
  dark: curStyle.getPropertyValue('--bs-dark'),
}

const dockLayout = {ref: null as any as DockLayout};

export interface WindowInfo {
  onClose?: () => void,
  onOpen?: () => void,
  content: ReactNode,
  title?: string,
  closable?: boolean
}

function WrappedContentNode(props: { info: WindowInfo }) {
  const {info} = props;

  useEffect(() => {
    info.onOpen && info.onOpen();
    return () => {
      info.onClose && info.onClose();
    };
  }, []);

  return <StrictMode>{info.content}</StrictMode>
}

export function CreateWindow(key: string, info: WindowInfo) {
  const data: TabData = {
    title: info.title ?? key,
    closable: info.closable != undefined ? info.closable : true,
    id: key,
    cached: true,
    content: <WrappedContentNode info={info}/>
  };

  if (IsWindowOpen(key))
    dockLayout.ref.updateTab(key, data, true);
  else
    dockLayout.ref.dockMove(data, null, "float");
}

export function CloseWindow(key: string): boolean {
  const data = dockLayout.ref.find(key) as null | PanelData | TabData;
  if (data == null)
    return false;

  dockLayout.ref.dockMove(data, null, 'remove');
  return true;
}

export function IsWindowOpen(key: string): boolean {
  return dockLayout.ref.find(key) != null;
}

function DockWrapper(prop: {
  children?: React.ReactNode;
}) {
  return <div className='p-2'>
    {prop.children}
  </div>
}

interface SystemInfo {
  alias: string,
  description: string,
  epoch: bigint,
  hostname: string,
  num_cores: number
}

const initialWidgetStatus = JSON.parse(
  localStorage.getItem('PerfkitWeb/widgetStatus/2') ?? "{}") as { [key: string]: boolean };
const savedLayout = {ref: {} as LayoutBase};

function App() {
  const dockRef = useRef(null as any as DockLayout);
  const enableStatus = useRef({})
  const [sysInfo, setSysInfo] = useState(null as SystemInfo | null);
  const widgetOpenStatus = useRef(initialWidgetStatus);
  const defaultLayout = useMemo(() => {
    return {
      dockbox: {
        mode: 'horizontal',
        id: 'panel-main',
        children: [
          {
            tabs: [
              {
                id: 'root/Terminal',
                title: 'Terminal',
                content: <div>{widgetOpenStatus.current['Terminal'] ? "MON" : "TWO"}</div>,
              },
              {
                id: 'root/Configs',
                title: 'Configs',
                content: <div></div>,
              },
              {
                id: 'root/Traces',
                title: 'Traces',
                content: <div></div>,
              },
              {
                id: 'root/System',
                title: 'System',
                content: <div></div>,
              },
              {
                id: 'root/Graphics',
                title: 'Graphics',
                content: <div></div>,
              },
            ]
          }
        ]
      }
    } as LayoutData;
  }, []);

  useEffect(() => {
    // Load layout and make hooks
    dockLayout.ref = dockRef.current;

    console.log("Loading layout");
    const layoutStatus = localStorage.getItem('PerfkitWeb/LayoutStatus/2');
    try {
      const layout = JSON.parse(layoutStatus ?? "") as LayoutBase;
      dockRef.current.loadLayout(layout);
      console.log(layout);
    } catch {
      JSON.parse(`"parse"`);
    }

    window.onbeforeunload = ev => {
      console.log("Saving status");

      localStorage.setItem('PerfkitWeb/LayoutStatus/2', JSON.stringify(dockRef.current.saveLayout()));
      localStorage.setItem('PerfkitWeb/widgetStatus/2', JSON.stringify(widgetOpenStatus.current));
    };

    // Load system information
    (async () => {
      const sysInfo = JSON.parse(await (await fetch('/api/system_info')).text()) as SystemInfo;
      setSysInfo(sysInfo);

      // TODO: Save widget open status
    })();
  }, []);

  useEffect(() => {
    if (sysInfo == null)
      return;

    document.title = `${sysInfo.alias}@${sysInfo.hostname} - Perfkit Web Dashboard`;
  }, [sysInfo]);

  function NavToggle(props: { itemTitle: string, iconClass: string, widgetFactory: () => ReactNode }) {
    const [isOpen, setIsOpen] = useState(false);
    const wndId = 'root/' + props.itemTitle;
    const refNode = useRef(null as ReactNode);

    useEffect(() => {
      dockLayout.ref = dockRef.current;

      // Initial load operation ... check if given element is open!
      if (props.itemTitle in widgetOpenStatus.current && widgetOpenStatus.current[props.itemTitle]) {
        console.log(`Widget ${props.itemTitle} is initailly open`);
        setIsOpen(true);
      }
    }, []);

    useEffect(() => {
      if (isOpen) {
        refNode.current = props.widgetFactory();
        dockRef.current.updateTab(wndId, {
          content: refNode.current as ReactElement,
          title: props.itemTitle,
          cached: true,
          id: 'root/' + props.itemTitle
        });
        widgetOpenStatus.current[props.itemTitle] = true;
      } else {
        dockRef.current.updateTab(wndId, {
          content: <div className='h-100 d-flex justify-content-center align-items-center'>
            <div className='btn text-success ri-links-line px-3 text-center'
                 style={{fontSize: '2rem'}}
                 onClick={() => setIsOpen(true)}>
            </div>
          </div>,
          title: props.itemTitle,
          cached: true,
          id: 'root/' + props.itemTitle
        }, false);
        widgetOpenStatus.current[props.itemTitle] = false;
      }
    }, [isOpen]);

    function OnClick() {
      setIsOpen(v => !v);
    }

    return <div className={(isOpen ? 'btn btn-outline-success' : 'btn') + ' mx-1 px-4'}
                title={props.itemTitle}
                onClick={OnClick}>
      <div className='d-flex flex-row align-items-center gap-1'>
        <i className={props.iconClass} style={{fontSize: '1.3rem'}}/>
        <div className='fw-bold'
             style={{maxWidth: isOpen ? '12ch' : '0', overflow: 'hidden', transition: '0.5s'}}>{props.itemTitle}</div>
      </div>
    </div>
  }

  return (
    <div className='App d-flex flex-column'>
      <Nav className="px-3" style={{overflowX: 'auto'}}>
        <Navbar>
          <a className='navbar-brand' href='/'>
            <img src={'logo.png'} alt='perfkit-logo' style={{maxWidth: '2em'}}/>
          </a>
          <NavToggle itemTitle={'Terminal'} iconClass={'ri-terminal-fill'}
                     widgetFactory={() => <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>}/>
          <NavToggle itemTitle={'Configs'} iconClass={'ri-tools-fill'}
                     widgetFactory={() => <ConfigPanel socketUrl={socketUrlPrefix + '/ws/config'}/>}/>
          <NavToggle itemTitle={'Traces'} iconClass={'ri-focus-3-line'}
                     widgetFactory={() => <TracePanel socketUrl={socketUrlPrefix + '/ws/trace'}/>}/>
          <NavToggle itemTitle={'Graphics'} iconClass={'ri-palette-line'}
                     widgetFactory={() => <GraphicPanel socketUrl={socketUrlPrefix + '/ws/graphic'}/>}/>
          <NavToggle itemTitle={'System'} iconClass={'ri-cpu-line'}
                     widgetFactory={() => <div>FF</div>}/>
          <span className='ms-2 me-4' style={{borderRight: '1px dimgray solid', height: '100%'}}/>
          <span className='navbar-brand text-light ms-2'>
              {sysInfo ? `${sysInfo.alias}@${sysInfo.hostname}` : "Perfkit"}
            </span>
        </Navbar>
      </Nav>
      <div className="flex-fill">
        <DockLayout
          ref={dockRef}
          defaultLayout={defaultLayout}
          style={{
            width: '100%',
            height: '100%',
          }}
        />
      </div>
    </div>
  );
}

export default App;
