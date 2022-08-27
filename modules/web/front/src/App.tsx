import {createContext, useEffect, useState} from 'react';
import './App.scss';
import 'bootstrap/dist/css/bootstrap.min.css';
import 'remixicon/fonts/remixicon.css'
import Terminal from "./comp/Terminal";
import {Button} from 'react-bootstrap'

import {Container, Row, Col} from "react-bootstrap";

export const authContext = createContext(null as string | null);
export const socketUrlPrefix = process.env.NODE_ENV === "development" ? "ws://localhost:10021" : window.location.host

interface ToggleRibbonProps {
  enableState: boolean
  setEnableState: (v: boolean | ((v: boolean) => boolean)) => void;
  iconClass: string;
  labelText: string;
  toolTip: string;
}

function ToggleRibbon(prop: ToggleRibbonProps) {
  const [mouseHover, setMouseHover] = useState(false);

  useEffect(() => {  }, [prop.enableState]);

  return <div className='d-grid'>
    <Button
      title={prop.toolTip}
      variant={prop.enableState ? 'success':'outline-secondary'}
      onClick={() => prop.setEnableState(v => !v)}
      onMouseEnter={() => setMouseHover(true)}
      onMouseLeave={() => setMouseHover(false)}>
      <i className={prop.iconClass + ' ToggleRibbon-icon'}/>{mouseHover &&
        <span className='ToggleRibbon-text'>{prop.labelText}</span>}
    </Button>
  </div>;
}

function App() {
  const [enableTerminal, setEnableTerminal] = useState(true);
  const [enableSystemInfo, setEnableSystemInfo] = useState(false);
  const [enableGraphics, setEnableGraphics] = useState(false);
  const [enableConfigs, setEnableConfigs] = useState(true);
  const [enableTraces, setEnableTraces] = useState(true);
  const [enablePlottings, setEnablePlottings] = useState(true);

  return (
    <div className='App'>
      <Row>
        <Col>
          <ToggleRibbon enableState={enableTerminal}
                        setEnableState={setEnableTerminal}
                        iconClass={'ri-terminal-window-fill'}
                        labelText={'Terminal'}
                        toolTip={'Show/Hide terminal'}/>
        </Col>
        <Col>
          <ToggleRibbon enableState={enableSystemInfo}
                        setEnableState={setEnableSystemInfo}
                        iconClass={'ri-checkbox-multiple-blank-fill'}
                        labelText={'System'}
                        toolTip={'Show/Hide system stat panel'}/>
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
          <ToggleRibbon enableState={enablePlottings}
                        setEnableState={setEnablePlottings}
                        iconClass={'ri-line-chart-line'}
                        labelText={'Plots'}
                        toolTip={'Show/Hide plots panel'}/>
        </Col>
      </Row>
      <Container fluid>
        <Row>
          <br/>
        </Row>
        <Row>
          <Col className={enableTerminal ? '' : 'd-none'} xl={8}>
            <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>
          </Col>
          <Col className={enableSystemInfo ? '' : 'd-none'}>
            <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>
          </Col>
        </Row>
        <Row>
          <Col>
            <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>
          </Col>
          <Col>
            <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}></Terminal>
          </Col>
          <Col>
            <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}></Terminal>
          </Col>
        </Row>
      </Container>
    </div>
  );
}

export default App;
