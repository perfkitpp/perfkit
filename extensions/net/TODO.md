
```text
              ┌────────────────┐
              │   <<if>>       │
              │   if_terminal  │
              └───────▲────────┘                0..1┌──────────────────┐
                      │<<impl>>                ┌────►   net::context   │
                      │    ┌───────────────┐   │    └──────────────────┘
                      └────┤ net::terminal ├───┘
                           └──┬────────────┘
                              │
                              │           |on_recv<msg_t>(route,[](msg_t const&){})
                 ┌────────────▼─────────┐ |send<>(route, fence, payload);
                 │   basic_dispatcher   │ |set_session_info(info))
                 └───────▲──────────▲───┘
                         │          │
                      <<derive>    <<derive>>
                         │          │
┌────────────────────────┴──┐   ┌───┴────────────────────────┐
│  provider_mode_dispatcher │   │   server_mode_dispatcher   │
└───────────────────────────┘   └────────────────────────────┘
 |set_apiserver(endpoint)        |bind(endpoint)
 
```
