import { Component } from 'solid-js';
import { Switch, SwitchControl, SwitchLabel, SwitchThumb } from './components/ui/switch';

interface MxSwitchProps {
	label: string;
	value: boolean;
	onChange?: (value: boolean) => void;
	disabled?: boolean;
}

export const MxSwitch : Component<MxSwitchProps> = (props) => {

	function onChange(value: boolean) {
		if(props.onChange) {
			props.onChange(value);
		}
	}

	return (
		<Switch class="flex items-center space-x-2" checked={props.value} onChange={onChange} disabled={props.disabled !== undefined && props.disabled}>
			<SwitchControl>
				<SwitchThumb/>
			</SwitchControl>
			<SwitchLabel>{props.label}</SwitchLabel>
		</Switch>
	);
};
