import {createContext, CSSProperties, useEffect, useMemo, useRef, useState} from 'react';
import './App.scss';
import Terminal from "./comp/Terminal";
import {Button, Container, Row, Col, ColProps} from "react-bootstrap";
import ConfigPanel from "./comp/ConfigPanel";
import TracePanel from './comp/TracePanel'
import {BoxData, DockLayout, LayoutBase, LayoutData, PanelData, TabData} from "rc-dock";

export const socketUrlPrefix = process.env.NODE_ENV === "development"
  ? `ws://${window.location.hostname}:15572`
  : "ws://" + window.location.host

interface ToggleRibbonProps {
  enableState: boolean
  setEnableState: (v: boolean | ((v: boolean) => boolean)) => void;
  iconClass: string;
  labelText: string;
  toolTip: string;
}

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

interface EnableStatus {
  terminal?: boolean;
  system?: boolean;
  graphics?: boolean;
  configs?: boolean;
  traces?: boolean;
  plottings?: boolean;
  layout?: DockLayout;
}

const initStateCfgStr = localStorage.getItem('PerfkitWeb/EnableStatus');
const initStateCfg = JSON.parse(initStateCfgStr || "{}") as EnableStatus;


function DockWrapper(prop: {
  children?: React.ReactNode;
}) {
  return <div className='p-2'>
    {prop.children}
  </div>
}

function App() {
  const dockRef = useRef(null as any as DockLayout);
  const enableStatus = useRef({})
  const defaultLayout = useMemo(() => {
    return {
      dockbox: {
        mode: 'horizontal',
        id: 'panel-main',
        children: [
          {
            tabs: [
              {
                id: 'tab-term',
                title: 'Terminal',
                content:
                  <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>,
                cached: true
              },
              {
                id: 'tab-cfg',
                title: 'Configs',
                content: <ConfigPanel socketUrl={socketUrlPrefix + '/ws/config'}/>,
                cached: true
              },
              {
                id: 'tab-trace',
                title: 'Traces',
                content: <TracePanel socketUrl={socketUrlPrefix + '/ws/trace'}/>,
                cached: true
              },
            ]
          }
        ]
      }
    } as LayoutData;
  }, []);

  useEffect(() => {
    const layoutStatus = localStorage.getItem('PerfkitWeb/LayoutStatus');
    try {
      const layout = JSON.parse(layoutStatus ?? "") as LayoutBase;
      dockRef.current.loadLayout(layout);
    } catch {

    }

    window.onbeforeunload = ev => {
      const layout = dockRef.current.saveLayout();
      localStorage.setItem('PerfkitWeb/LayoutStatus', JSON.stringify(layout));
    };
  }, []);

  return (
    <div className='App d-flex flex-column'>
      <DockLayout
        ref={dockRef}
        defaultLayout={defaultLayout}
        style={{
          position: "absolute",
          left: 10,
          top: 10,
          right: 10,
          bottom: 10,
        }}
      />
    </div>
  );
}

export default App;
