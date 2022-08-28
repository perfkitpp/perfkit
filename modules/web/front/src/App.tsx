import {createContext, useEffect, useState} from 'react';
import './App.scss';
import 'bootstrap/dist/css/bootstrap.min.css';
import 'remixicon/fonts/remixicon.css'
import Terminal from "./comp/Terminal";
import {Button, Container, Row, Col, ColProps} from "react-bootstrap";

export const socketUrlPrefix = process.env.NODE_ENV === "development" ? "ws://localhost:10021" : "ws://" + window.location.host

interface ToggleRibbonProps {
  enableState: boolean
  setEnableState: (v: boolean | ((v: boolean) => boolean)) => void;
  iconClass: string;
  labelText: string;
  toolTip: string;
}

function ToggleRibbon(prop: ToggleRibbonProps) {
  const [mouseHover, setMouseHover] = useState(false);

  useEffect(() => {
  }, [prop.enableState]);

  return <div className='d-grid'>
    <Button
      title={prop.toolTip}
      variant={prop.enableState ? 'primary' : 'outline-secondary'}
      onClick={() => prop.setEnableState(v => !v)}
      onMouseEnter={() => setMouseHover(true)}
      onMouseLeave={() => setMouseHover(false)}>
      <i className={prop.iconClass + ' ToggleRibbon-icon'}/>{mouseHover &&
        <span className='ToggleRibbon-text'>{prop.labelText}</span>}
    </Button>
  </div>;
}

interface ModulePanelProps {
  title?: string;
  iconClass?: string;
  enabled?: boolean;
  column?: ColProps;

  children?: React.ReactNode;
}

export function ModulePanelCol(prop: ModulePanelProps) {
  return (
    <Col {...prop.column}
         style={{
           display: prop.enabled == null || prop.enabled ? 'block' : 'none',
           overflowY: 'hidden',
           flexDirection: 'column'
         }}>
      <div className='ModulePanel d-flex flex-column'>
        <div className='text-center flex-grow-0'>
          <span className={`${prop.iconClass} fw-bold w-100`}
                style={{fontSize: '1.2em'}}>
          </span>
          <span className='ms-1 fw-bold' style={{fontSize: '1.2em'}}>
          {prop.title}
          </span>
        </div>
        <div style={{flex: '1', height: "100%", overflowY: 'auto'}}>
          {prop.children}
        </div>
      </div>
    </Col>
  );
}

interface EnableStatus {
  terminal: boolean;
  system: boolean;
  graphics: boolean;
  configs: boolean;
  traces: boolean;
  plottings: boolean;
}

const initStateCfgStr = localStorage.getItem('PerfkitWeb/EnableStatus');
const initStateCfg = JSON.parse(initStateCfgStr || "{}") as EnableStatus;

function App() {
  const [enableTerminal, setEnableTerminal] = useState((initStateCfg.terminal ??= true) as boolean);
  const [enableSystemInfo, setEnableSystemInfo] = useState((initStateCfg.system ??= true) as boolean);
  const [enableGraphics, setEnableGraphics] = useState((initStateCfg.graphics ??= false) as boolean);
  const [enableConfigs, setEnableConfigs] = useState((initStateCfg.configs ??= true) as boolean);
  const [enableTraces, setEnableTraces] = useState((initStateCfg.traces ??= true) as boolean);
  const [enablePlottings, setEnablePlottings] = useState((initStateCfg.plottings ??= false) as boolean);

  useEffect(() => {
    let obj = {} as EnableStatus;
    obj.terminal = enableTerminal;
    obj.system = enableSystemInfo;
    obj.graphics = enableGraphics;
    obj.configs = enableConfigs;
    obj.traces = enableTraces;
    obj.plottings = enablePlottings;
    localStorage.setItem('PerfkitWeb/EnableStatus', JSON.stringify(obj));
  }, [enableTerminal, enableSystemInfo, enableGraphics, enableConfigs, enableTraces, enablePlottings]);


  return (
    <div className='App d-flex flex-column'>
      <div className='border-bottom border-2 border-primary bg-primary bg-opacity-10 py-2 px-3 flex-grow-0'>
        <Row className=''>
          <Col>
            <ToggleRibbon enableState={enableTerminal}
                          setEnableState={setEnableTerminal}
                          iconClass={'ri-terminal-window-fill'}
                          labelText={'Terminal'}
                          toolTip={'Show/Hide terminal'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableGraphics}
                          setEnableState={setEnableGraphics}
                          iconClass={'ri-artboard-line'}
                          labelText={'Graphics'}
                          toolTip={'Show/Hide grahpics panel'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableConfigs}
                          setEnableState={setEnableConfigs}
                          iconClass={'ri-list-settings-fill'}
                          labelText={'Configs'}
                          toolTip={'Show/Hide terminal'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableTraces}
                          setEnableState={setEnableTraces}
                          iconClass={'ri-dashboard-2-line'}
                          labelText={'Traces'}
                          toolTip={'Show/Hide trace panel'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableSystemInfo}
                          setEnableState={setEnableSystemInfo}
                          iconClass={'ri-checkbox-multiple-blank-fill'}
                          labelText={'System'}
                          toolTip={'Show/Hide system stat panel'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enablePlottings}
                          setEnableState={setEnablePlottings}
                          iconClass={'ri-line-chart-line'}
                          labelText={'Plots'}
                          toolTip={'Show/Hide plots panel'}/>
          </Col>
        </Row>
      </div>
      <Container fluid className='mt-3 overflow-scroll flex-grow-1'>
        <Row className='my-1'>
          <ModulePanelCol column={{xxl: 0}} title='Terminal' iconClass='ri-terminal-line' enabled={enableTerminal}>
            <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>
          </ModulePanelCol>
          <ModulePanelCol column={{xxl: 0}} title='Grahpics' iconClass='ri-artboard-line' enabled={enableGraphics}>
            Graphics window will be placed here.
          </ModulePanelCol>
        </Row>
        <Row className='my-1'>
          <ModulePanelCol title='Configs' iconClass='ri-list-settings-fill' enabled={enableConfigs}>
            Configs window will be placed here.
          </ModulePanelCol>
          <ModulePanelCol title='Traces' iconClass='ri-artboard-line' enabled={enableTraces}>
            Traces window will be placed here.
          </ModulePanelCol>
          <ModulePanelCol title='System Status' iconClass='ri-checkbox-multiple-blank-fill' enabled={enableSystemInfo}>
            System Info window will be placed here.
          </ModulePanelCol>
        </Row>
        <Row className='my-1'>
          <ModulePanelCol title='Plotting' iconClass='ri-line-chart-line' enabled={enablePlottings}>
            Plotting window will be placed here.
          </ModulePanelCol>
        </Row>
      </Container>
    </div>
  );
}

export default App;
