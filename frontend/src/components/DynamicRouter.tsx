import { Route } from "@solidjs/router";
import { Component, JSXElement, createContext, createSignal, Accessor, For, useContext } from "solid-js";

export interface MxDynamicRoute {
	path: string;
	component: Component;
};

export interface MxDynamicRouterContext {
	routes: Accessor<Array<MxDynamicRoute>>;
	addRoute: (path: string, component: Component) => void;
	removeRoute: (path: string) => void;
};

export const DynamicRouterContext = createContext<MxDynamicRouterContext>();
export const DynamicRouterProvider : Component<{ children: JSXElement }> = (props) => {
	const [routes, setRoutes] = createSignal<Array<MxDynamicRoute>>(new Array<MxDynamicRoute>());

	const addRoute = (path: string, component: Component) => {
		if(routes().find(o => o.path === '/dynamic' + path) !== undefined) {
			console.warn('Cannot add dynamic path. Already exists.');
			return;
		}

		setRoutes((p) => [...p, { path: '/dynamic' + path, component: component }]);
	};

	const removeRoute = (path: string) => {
		setRoutes((p) => {
			const element = p.find(route => route.path == '/dynamic' + path);
			if(element) {
				const idx = p.indexOf(element, 0);
				if(idx > -1) {
					return p.splice(idx, 1);
				}
			}
			return p;
		});
	};

	return <DynamicRouterContext.Provider value={{ routes: routes, addRoute: addRoute, removeRoute: removeRoute }}>{props.children}</DynamicRouterContext.Provider>
};

export const DynamicRouter : Component = () => {
	const { routes } = useContext(DynamicRouterContext) as MxDynamicRouterContext;
	return (
		<For each={routes()}>{(route) => {
			return <Route path={route.path} component={route.component}/>
		}}</For>

	);
};
