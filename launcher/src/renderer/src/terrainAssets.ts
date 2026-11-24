import chernarusImage from './assets/chernarus.png'
import livoniaImage from './assets/livonia.png'
import sakhalImage from './assets/sakhal.png'
import nasdaraImage from './assets/nasdara.png'
import chernarus from '../../../resources/chernarus-places.json'
import livonia from '../../../resources/livonia-places.json'
import sakhal from '../../../resources/sakhal-places.json'
import nasdara from '../../../resources/nasdara-places.json'

export const terrainAssets = {
  chernarusplus: { ...chernarus, image: chernarusImage, sunrise: 259, sunset: 1182 },
  enoch: { ...livonia, image: livoniaImage },
  sakhal: { ...sakhal, image: sakhalImage },
  nasdara: { ...nasdara, image: nasdaraImage },
}
